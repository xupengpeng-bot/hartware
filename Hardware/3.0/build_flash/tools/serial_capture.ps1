param(
    [string]$Port,
    [int]$Baud = 9600,
    [string]$OutputDir = "",
    [string]$SessionName = "",
    [int]$ReadTimeoutMs = 200,
    [switch]$Append,
    [switch]$ListPorts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-SafeName {
    param([string]$Name)

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return "serial"
    }

    return ([regex]::Replace($Name.Trim(), '[^A-Za-z0-9._-]+', '_').Trim('_'))
}

function Convert-BytesToAsciiDebug {
    param(
        [byte[]]$Bytes,
        [int]$Count
    )

    $sb = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $Count; $i++) {
        $b = $Bytes[$i]
        if (($b -ge 0x20) -and ($b -le 0x7E)) {
            [void]$sb.Append([char]$b)
        } elseif ($b -eq 0x0D) {
            [void]$sb.Append("`r")
        } elseif ($b -eq 0x0A) {
            [void]$sb.Append("`n")
        } elseif ($b -eq 0x09) {
            [void]$sb.Append("`t")
        } else {
            [void]$sb.AppendFormat("<{0:X2}>", $b)
        }
    }
    return $sb.ToString()
}

function Write-HexDump {
    param(
        [System.IO.StreamWriter]$Writer,
        [byte[]]$Bytes,
        [int]$Count,
        [ref]$Offset
    )

    $index = 0
    while ($index -lt $Count) {
        $lineCount = [Math]::Min(16, $Count - $index)
        $hexParts = New-Object System.Collections.Generic.List[string]
        $asciiSb = New-Object System.Text.StringBuilder

        for ($j = 0; $j -lt 16; $j++) {
            if ($j -lt $lineCount) {
                $b = $Bytes[$index + $j]
                [void]$hexParts.Add(("{0:X2}" -f $b))
                if (($b -ge 0x20) -and ($b -le 0x7E)) {
                    [void]$asciiSb.Append([char]$b)
                } else {
                    [void]$asciiSb.Append(".")
                }
            } else {
                [void]$hexParts.Add("  ")
                [void]$asciiSb.Append(" ")
            }
        }

        $hexText = ($hexParts -join ' ')
        $Writer.WriteLine(("{0:X8}  {1}  |{2}|" -f $Offset.Value, $hexText, $asciiSb.ToString()))
        $Offset.Value += $lineCount
        $index += $lineCount
    }
    $Writer.Flush()
}

if ($ListPorts) {
    $ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
    if ($ports.Length -eq 0) {
        Write-Host "(no COM ports found)"
    } else {
        $ports | ForEach-Object { Write-Host $_ }
    }
    exit 0
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    Write-Error "Missing -Port. Example: -Port COM7"
    exit 2
}

if ($Baud -le 0) {
    Write-Error "Baud must be a positive integer."
    exit 2
}

$buildFlashDir = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $buildFlashDir "logs"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$safePort = Get-SafeName $Port
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$baseName = if ([string]::IsNullOrWhiteSpace($SessionName)) {
    "serial_${safePort}_${timestamp}"
} else {
    Get-SafeName $SessionName
}

$textPath = Join-Path $OutputDir ($baseName + ".txt")
$hexPath = Join-Path $OutputDir ($baseName + ".hex.txt")
$rawPath = Join-Path $OutputDir ($baseName + ".raw.bin")

$textAppend = $Append.IsPresent -and (Test-Path -LiteralPath $textPath)
$rawMode = if ($Append.IsPresent -and (Test-Path -LiteralPath $rawPath)) {
    [System.IO.FileMode]::Append
} else {
    [System.IO.FileMode]::Create
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
$textWriter = [System.IO.StreamWriter]::new($textPath, $textAppend, $utf8)
$hexWriter = [System.IO.StreamWriter]::new($hexPath, $textAppend, $utf8)
$rawStream = [System.IO.File]::Open($rawPath, $rawMode, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)

$serial = $null
$bytesCaptured = 0L
$hexOffset = 0L
$buffer = New-Object byte[] 4096

try {
    $textWriter.WriteLine("=== serial capture started ===")
    $textWriter.WriteLine("port=$Port")
    $textWriter.WriteLine("baud=$Baud")
    $textWriter.WriteLine("local_time=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff zzz')")
    $textWriter.WriteLine("raw_file=$rawPath")
    $textWriter.WriteLine("hex_file=$hexPath")
    $textWriter.WriteLine("")
    $textWriter.Flush()

    $hexWriter.WriteLine("=== serial hex capture started ===")
    $hexWriter.WriteLine("port=$Port")
    $hexWriter.WriteLine("baud=$Baud")
    $hexWriter.WriteLine("local_time=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff zzz')")
    $hexWriter.WriteLine("raw_file=$rawPath")
    $hexWriter.WriteLine("")
    $hexWriter.Flush()

    $serial = [System.IO.Ports.SerialPort]::new($Port, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.Handshake = [System.IO.Ports.Handshake]::None
    $serial.ReadTimeout = $ReadTimeoutMs
    $serial.WriteTimeout = 1000
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    $serial.Open()

    Write-Host "[serial] Port: $Port"
    Write-Host "[serial] Baud: $Baud"
    Write-Host "[serial] Text log: $textPath"
    Write-Host "[serial] Hex log : $hexPath"
    Write-Host "[serial] Raw log : $rawPath"
    Write-Host "[serial] Capturing... Press Ctrl+C to stop."
    Write-Host ""

    while ($true) {
        try {
            $read = $serial.Read($buffer, 0, $buffer.Length)
            if ($read -le 0) {
                continue
            }

            $rawStream.Write($buffer, 0, $read)
            $rawStream.Flush()

            $chunkText = Convert-BytesToAsciiDebug -Bytes $buffer -Count $read
            $textWriter.Write($chunkText)
            $textWriter.Flush()
            Write-HexDump -Writer $hexWriter -Bytes $buffer -Count $read -Offset ([ref]$hexOffset)

            [Console]::Out.Write($chunkText)
            $bytesCaptured += $read
        } catch [System.TimeoutException] {
            continue
        }
    }
} finally {
    try {
        if ($textWriter -ne $null) {
            $textWriter.WriteLine("")
            $textWriter.WriteLine("")
            $textWriter.WriteLine("=== serial capture stopped ===")
            $textWriter.WriteLine("local_time=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff zzz')")
            $textWriter.WriteLine("bytes_captured=$bytesCaptured")
            $textWriter.Flush()
        }
    } catch {
    }

    try {
        if ($hexWriter -ne $null) {
            $hexWriter.WriteLine("")
            $hexWriter.WriteLine("=== serial hex capture stopped ===")
            $hexWriter.WriteLine("local_time=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff zzz')")
            $hexWriter.WriteLine("bytes_captured=$bytesCaptured")
            $hexWriter.Flush()
        }
    } catch {
    }

    if ($serial -ne $null) {
        try {
            if ($serial.IsOpen) {
                $serial.Close()
            }
        } catch {
        }
        $serial.Dispose()
    }

    if ($rawStream -ne $null) {
        $rawStream.Dispose()
    }

    if ($hexWriter -ne $null) {
        $hexWriter.Dispose()
    }

    if ($textWriter -ne $null) {
        $textWriter.Dispose()
    }

    Write-Host ""
    Write-Host "[serial] Capture stopped. Bytes: $bytesCaptured"
    Write-Host "[serial] Text log: $textPath"
    Write-Host "[serial] Hex log : $hexPath"
    Write-Host "[serial] Raw log : $rawPath"
}
