param([string]$BuildDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build'))

$ErrorActionPreference = 'Stop'
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$suffix = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { '.exe' } else { '' }
$program = Join-Path $BuildDirectory ('machineguard' + $suffix)
$failureProgram = Join-Path $BuildDirectory ('machineguard_failure' + $suffix)
$testRoot = Join-Path $BuildDirectory ('integration-' + [guid]::NewGuid().ToString('N'))
$header = 'sequence,event,previous_state,current_state,temperature_c,motor_on'
$script:scenarioCount = 0
New-Item -ItemType Directory -Path $testRoot | Out-Null
foreach ($binary in @($program, $failureProgram)) {
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Missing executable: $binary" }
}

function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Pass([string]$Message) {
    $script:scenarioCount++
    Write-Output "PASS: $Message"
}

function New-Scenario([string]$Name) {
    $directory = Join-Path $testRoot $Name
    New-Item -ItemType Directory -Path $directory | Out-Null
    return $directory
}

function Run-Program([string]$Directory, [string]$InputText,
                     [string]$Binary = $program, [int]$FailAt = 3) {
    # 使用獨立工作目錄；測試不會修改使用者的 events.csv。
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Binary
    $startInfo.WorkingDirectory = $Directory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.EnvironmentVariables['MACHINEGUARD_TEST_FAIL_AT'] = $FailAt.ToString()
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    try {
        Assert ($process.Start()) 'Cannot start test program.'
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.StandardInput.Write($InputText)
        $process.StandardInput.Close()
        if (-not $process.WaitForExit(10000)) {
            $process.Kill()
            throw 'Test program exceeded 10-second timeout.'
        }
        return @{ Code = $process.ExitCode; Out = $stdout.GetAwaiter().GetResult(); Err = $stderr.GetAwaiter().GetResult() }
    } finally {
        $process.Dispose()
    }
}

function Read-Trace([string]$Directory) {
    return @(Import-Csv -LiteralPath (Join-Path $Directory 'events.csv') | ForEach-Object {
        '{0} {1} {2} {3} {4} {5}' -f $_.sequence, $_.event, $_.previous_state,
                                  $_.current_state, $_.temperature_c, $_.motor_on
    })
}

function Test-Trace([string]$Name, [string]$InputText, [string[]]$Expected) {
    $directory = New-Scenario $Name
    $result = Run-Program $directory $InputText
    Assert ($result.Code -eq 0) "$Name failed: $($result.Err)"
    $actual = @(Read-Trace $directory)
    Assert (($actual -join '|') -eq ($Expected -join '|')) "${Name}: unexpected CSV trace."
    Assert ($result.Out.Contains('Program ended; simulated motor OFF.')) "${Name}: missing orderly shutdown."
    Pass $Name
}

$normal = New-Scenario 'full-recovery'
$result = Run-Program $normal "1`n2`n4`n3`n1`n4`n1`n6`nx`n5`n0`n"
Assert ($result.Code -eq 0) 'Recovery flow failed.'
$expected = @(
    '1 SESSION_START IDLE IDLE 30 0',
    '2 START IDLE RUNNING 30 1',
    '3 OVERHEAT RUNNING FAULT 85 0',
    '4 RESET_REJECTED_HOT FAULT FAULT 85 0',
    '5 COOL FAULT FAULT 40 0',
    '6 START_REJECTED_NOT_IDLE FAULT FAULT 40 0',
    '7 RESET FAULT IDLE 40 0',
    '8 START IDLE RUNNING 40 1',
    '9 STOP RUNNING IDLE 40 0',
    '10 UNKNOWN_COMMAND IDLE IDLE 40 0',
    '11 STATUS IDLE IDLE 40 0',
    '12 QUIT IDLE IDLE 40 0'
)
Assert (((Read-Trace $normal) -join '|') -eq ($expected -join '|')) 'Incorrect recovery trace.'
Pass 'full recovery with rejected commands and unknown input'

$logPath = Join-Path $normal 'events.csv'
$before = [IO.File]::ReadAllText($logPath)
$result = Run-Program $normal "1`n"
$after = [IO.File]::ReadAllText($logPath)
$rows = @(Import-Csv -LiteralPath $logPath)
Assert ($result.Code -eq 0 -and $after.StartsWith($before)) 'Append changed existing data.'
Assert ([regex]::Matches($after, '(?m)^sequence,event,').Count -eq 1) 'Duplicate header.'
Assert ($rows.Count -eq 15 -and $rows[12].sequence -eq '1' -and
        $rows[12].event -eq 'SESSION_START' -and $rows[14].event -eq 'INPUT_END' -and
        $rows[14].current_state -eq 'IDLE' -and $rows[14].motor_on -eq '0') 'Incorrect appended session.'
Pass 'append, one header, new session and EOF shutdown'

foreach ($case in @(
    @{ Name = 'bad-header'; Content = "unrelated,data`r`nkeep,this`r`n" },
    @{ Name = 'partial-row'; Content = "$header`r`n1,SESSION_START" }
)) {
    $directory = New-Scenario $case.Name
    $path = Join-Path $directory 'events.csv'
    [IO.File]::WriteAllText($path, $case.Content, [Text.UTF8Encoding]::new($false))
    $original = [Convert]::ToBase64String([IO.File]::ReadAllBytes($path))
    $result = Run-Program $directory "1`n0`n"
    Assert ($result.Code -eq 1 -and -not $result.Out.Contains('motor=ON')) 'Invalid log did not block startup.'
    Assert ([Convert]::ToBase64String([IO.File]::ReadAllBytes($path)) -eq $original) 'Invalid file was modified.'
    Pass $case.Name
}

$blocked = New-Scenario 'blocked-path'
New-Item -ItemType Directory -Path (Join-Path $blocked 'events.csv') | Out-Null
$result = Run-Program $blocked "1`n0`n"
Assert ($result.Code -eq 1 -and -not $result.Out.Contains('motor=ON')) 'Open failure did not block startup.'
Pass 'open failure prevents startup'

$failure = New-Scenario 'write-failure-running'
$result = Run-Program $failure "1`n5`n0`n" $failureProgram 3
$states = @([regex]::Matches($result.Out, 'state=(\w+) motor=(\w+)') | ForEach-Object {
    $_.Groups[1].Value + ' ' + $_.Groups[2].Value
})
Assert ($result.Code -eq 1 -and ($states -join '|') -eq 'IDLE OFF|RUNNING ON|IDLE OFF' -and
        $result.Err.Contains('Event logging failed')) 'Write failure did not stop running equipment.'
Pass 'write failure stops simulated motor'

Test-Trace 'quit-while-running' "1`n0`n" @(
    '1 SESSION_START IDLE IDLE 30 0', '2 START IDLE RUNNING 30 1', '3 QUIT RUNNING IDLE 30 0')
Test-Trace 'eof-without-input' '' @(
    '1 SESSION_START IDLE IDLE 30 0', '2 INPUT_END IDLE IDLE 30 0')
Test-Trace 'reset-without-fault' "4`n1`n4`n0`n" @(
    '1 SESSION_START IDLE IDLE 30 0', '2 RESET_REJECTED_NO_FAULT IDLE IDLE 30 0',
    '3 START IDLE RUNNING 30 1', '4 RESET_REJECTED_NO_FAULT RUNNING RUNNING 30 1',
    '5 QUIT RUNNING IDLE 30 0')
Test-Trace 'repeated-start' "1`n1`n0`n" @(
    '1 SESSION_START IDLE IDLE 30 0', '2 START IDLE RUNNING 30 1',
    '3 START_REJECTED_NOT_IDLE RUNNING RUNNING 30 1', '4 QUIT RUNNING IDLE 30 0')
Test-Trace 'stop-preserves-fault' "2`n6`n0`n" @(
    '1 SESSION_START IDLE IDLE 30 0', '2 OVERHEAT IDLE FAULT 85 0',
    '3 STOP FAULT FAULT 85 0', '4 QUIT FAULT FAULT 85 0')
Test-Trace 'whitespace-and-multiple-characters' " `n`t12`n 3 4 0`n" @(
    '1 SESSION_START IDLE IDLE 30 0', '2 START IDLE RUNNING 30 1',
    '3 OVERHEAT RUNNING FAULT 85 0', '4 COOL FAULT FAULT 40 0',
    '5 RESET FAULT IDLE 40 0', '6 QUIT IDLE IDLE 40 0')

foreach ($case in @(
    @{ Name = 'empty-existing-file'; Content = '' },
    @{ Name = 'header-only'; Content = "$header`r`n" },
    @{ Name = 'lf-header'; Content = "$header`n" }
)) {
    $directory = New-Scenario $case.Name
    $path = Join-Path $directory 'events.csv'
    [IO.File]::WriteAllText($path, $case.Content, [Text.UTF8Encoding]::new($false))
    $result = Run-Program $directory "0`n"
    Assert ($result.Code -eq 0) 'Compatible existing file was rejected.'
    Assert (((Read-Trace $directory) -join '|') -eq
        '1 SESSION_START IDLE IDLE 30 0|2 QUIT IDLE IDLE 30 0') 'Incorrect new session.'
    Assert ([regex]::Matches([IO.File]::ReadAllText($path), '(?m)^sequence,event,').Count -eq 1) 'Header repeated.'
    Pass $case.Name
}

$initialFailure = New-Scenario 'initial-write-failure'
$result = Run-Program $initialFailure "1`n0`n" $failureProgram 1
Assert ($result.Code -eq 1 -and -not $result.Out.Contains('motor=ON') -and
        $result.Err.Contains('Cannot write initial event')) 'Initial write failure did not prevent startup.'
Pass 'initial write failure prevents startup'

$finalFailure = New-Scenario 'final-write-failure'
$result = Run-Program $finalFailure "1`n0`n" $failureProgram 3
Assert ($result.Code -eq 1 -and $result.Out.Contains('state=IDLE motor=OFF') -and
        $result.Err.Contains('Cannot write final event')) 'Final write failure not reported after stop.'
Pass 'final write failure reported after motor off'

Test-Trace 'ignore-input-after-quit' "0`n1`n2`n" @(
    '1 SESSION_START IDLE IDLE 30 0', '2 QUIT IDLE IDLE 30 0')

Assert ($scenarioCount -eq 18) 'Integration scenario count changed unexpectedly.'
Write-Output "$scenarioCount integration scenarios passed."
Write-Output "Test artifacts: $testRoot"
