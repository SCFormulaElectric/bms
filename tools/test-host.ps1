$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root '.build\host-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null

$tests = @(
    @{
        Name = 'ams_config'
        Sources = @('tests\host\test_ams_config.c',
            'Core\Src\Ams\ams_config.c')
    },
    @{
        Name = 'ams_controller'
        Sources = @('tests\host\test_ams_controller.c',
            'Core\Src\Ams\ams_controller.c', 'Core\Src\Ams\ams_config.c')
    },
    @{
        Name = 'ams_battery'
        Sources = @('tests\host\test_ams_battery.c',
            'Core\Src\Ams\ams_battery.c', 'Core\Src\Ams\ams_config.c')
    },
    @{
        Name = 'bq796xx_protocol'
        Sources = @('tests\host\test_bq796xx_protocol.c',
            'Core\Src\Ams\bq796xx_protocol.c')
    },
    @{
        Name = 'adc_snapshot'
        Sources = @('tests\host\test_adc_snapshot.c',
            'Core\Src\Peripherals\adc_snapshot.c')
    }
)

Push-Location $root
try {
    foreach ($test in $tests) {
        $executable = Join-Path $output ($test.Name + '.exe')
        & gcc '-std=c11' '-Wall' '-Wextra' '-Werror' '-ICore/Inc' @($test.Sources) '-o' $executable
        if ($LASTEXITCODE -ne 0) {
            throw ('Host test compilation failed: ' + $test.Name)
        }
        & $executable
        if ($LASTEXITCODE -ne 0) {
            throw ('Host test failed: ' + $test.Name)
        }
        Write-Host ('PASS ' + $test.Name)
    }
} finally {
    Pop-Location
}
