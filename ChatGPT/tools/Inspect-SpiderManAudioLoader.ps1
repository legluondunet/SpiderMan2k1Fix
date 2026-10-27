<#
.SYNOPSIS
  Static native x86 audio loader investigation for Spider-Man PC (2001).
.DESCRIPTION
  PowerShell 5.1+; no Python, Wine, DLL injection or executable modification.
  Verifies the exact original SHA256 + size, parses PE32/i386, validates
  known original instruction signatures, lists possible rel32 x86 CALLs
  and 2048-byte constants, and optionally invokes objdump/llvm-objdump.
  Candidate bytes/calls are NOT proof of executable instruction boundaries.
.EXAMPLE
  .\Inspect-SpiderManAudioLoader.ps1 -ExePath 'C:\Games\Spider-Man\SpideyPC.exe'
.EXAMPLE
  .\Inspect-SpiderManAudioLoader.ps1 -ExePath '.\SpideyPC.exe' -ObjdumpPath 'C:\LLVM\bin\llvm-objdump.exe'
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateNotNullOrEmpty()]
    [string]$ExePath,

    [string]$OutputPath = 'spiderman-audio-root-disassembly.txt',

    [string]$ObjdumpPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expectedSize = 1507328
$expectedSha256 = '0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea'
$expectedImageBase = [long]0x00400000
$windowStart = [long]0x005039D0
$windowEnd   = [long]0x00504340
$maxCandidateCalls = 150
$maxPatternAddresses = 30

function Format-HexVa {
    param([long]$Value)
    return ('0x{0:X8}' -f $Value)
}

function Read-U16 {
    param([byte[]]$Bytes, [long]$Offset)
    if ($Offset -lt 0 -or $Offset + 2 -gt $Bytes.Length) {
        throw "PE16 read beyond file at offset $Offset"
    }
    return [BitConverter]::ToUInt16($Bytes, [int]$Offset)
}

function Read-U32 {
    param([byte[]]$Bytes, [long]$Offset)
    if ($Offset -lt 0 -or $Offset + 4 -gt $Bytes.Length) {
        throw "PE32 read beyond file at offset $Offset"
    }
    return [BitConverter]::ToUInt32($Bytes, [int]$Offset)
}

function Get-BytesAtVa {
    param([long]$Address, [int]$Count)
    if ($Count -lt 1 -or $Address -lt $script:imageBase) {
        throw "Invalid address/count: $(Format-HexVa $Address) / $Count"
    }
    $rva = $Address - $script:imageBase
    foreach ($section in $script:sections) {
        $sectionRva = [long]$section.Rva
        $rawSize = [long]$section.RawSize
        if ($rva -ge $sectionRva -and $rva + $Count -le $sectionRva + $rawSize) {
            $fileOffset = [long]$section.RawOffset + ($rva - $sectionRva)
            if ($fileOffset -lt 0 -or $fileOffset + $Count -gt $script:exeBytes.Length) {
                throw "Unmapped file offset for VA $(Format-HexVa $Address)"
            }
            $result = New-Object byte[] $Count
            [Array]::Copy($script:exeBytes, [int]$fileOffset, $result, 0, $Count)
            return ,$result
        }
    }
    throw "Unmapped raw VA $(Format-HexVa $Address) (+$Count)"
}

# Read-only verification MUST precede all report writes and disassembly.
$resolvedExe = (Resolve-Path -LiteralPath $ExePath -ErrorAction Stop).ProviderPath
if (-not [IO.File]::Exists($resolvedExe)) {
    throw "Not a file: $resolvedExe"
}
$script:exeBytes = [IO.File]::ReadAllBytes($resolvedExe)
if ($exeBytes.Length -ne $expectedSize) {
    throw "Unsupported SpideyPC.exe: $($exeBytes.Length) bytes; expected $expectedSize."
}
$sha = (Get-FileHash -LiteralPath $resolvedExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($sha -ne $expectedSha256) {
    throw "Unsupported SpideyPC.exe SHA-256: $sha"
}
if ($exeBytes[0] -ne 0x4D -or $exeBytes[1] -ne 0x5A) {
    throw "Invalid DOS MZ header."
}
$peOffset = [long](Read-U32 $exeBytes 0x3C)
if ($peOffset -lt 0x40 -or $peOffset + 0x100 -gt $exeBytes.Length) {
    throw "Invalid PE header offset."
}
if ($exeBytes[[int]$peOffset] -ne 0x50 -or
    $exeBytes[[int]($peOffset+1)] -ne 0x45 -or
    $exeBytes[[int]($peOffset+2)] -ne 0 -or
    $exeBytes[[int]($peOffset+3)] -ne 0) {
    throw "Missing PE signature."
}
$coff = $peOffset + 4
if ((Read-U16 $exeBytes $coff) -ne 0x014C) {
    throw "Unsupported architecture (expected x86 / i386)."
}
$sectionCount = [int](Read-U16 $exeBytes ($coff+2))
$optionalSize = [int](Read-U16 $exeBytes ($coff+16))
$optional = $coff + 20
if ((Read-U16 $exeBytes $optional) -ne 0x010B -or
    $optionalSize -lt 96 -or $optional + $optionalSize -gt $exeBytes.Length) {
    throw "Only PE32 executables are supported."
}
$script:imageBase = [long](Read-U32 $exeBytes ($optional+28))
if ($imageBase -ne $expectedImageBase) {
    throw "Unsupported PE ImageBase: $(Format-HexVa $imageBase)"
}
$sectionTable = $optional + $optionalSize
if ($sectionCount -lt 1 -or $sectionCount -gt 48 -or
    $sectionTable + 40 * $sectionCount -gt $exeBytes.Length) {
    throw "Invalid PE section table."
}
$script:sections = @()
for ($i = 0; $i -lt $sectionCount; $i++) {
    $entry = [long]$sectionTable + 40 * $i
    $name = [Text.Encoding]::ASCII.GetString($exeBytes, [int]$entry, 8).TrimEnd([char]0)
    $rva = [long](Read-U32 $exeBytes ($entry+12))
    $rawSize = [long](Read-U32 $exeBytes ($entry+16))
    $rawOffset = [long](Read-U32 $exeBytes ($entry+20))
    if ($rawOffset + $rawSize -gt $exeBytes.Length) {
        throw "Section $name exceeds file."
    }
    $script:sections += [pscustomobject]@{
        Name = $name
        Rva = $rva
        RawSize = $rawSize
        RawOffset = $rawOffset
    }
}
$textSections = @($sections | Where-Object { $_.Name -eq '.text' })
if ($textSections.Count -ne 1) {
    throw "Expected exactly one .text section."
}
$textSection = $textSections[0]
$textBaseVa = $imageBase + [long]$textSection.Rva

$known = @(
    [pscustomobject]@{ Name='voice initialization (signature-guarded)'; Va=0x005039F0; Signature=[byte[]]@(0x83,0xEC,0x24,0x56,0x57,0xB9,0x80,0,0,0) },
    [pscustomobject]@{ Name='DXSOUND_Open / duplicate path (signature-guarded)'; Va=0x00504110; Signature=[byte[]]@(0x8B,0x0D,0x20,0x79,0x6B,0,0x53,0x56,0x57) },
    [pscustomobject]@{ Name='native DXSOUND_Play (address only)'; Va=0x00504230; Signature=$null },
    [pscustomobject]@{ Name='native DXSOUND_Stop (signature-guarded)'; Va=0x005042A0; Signature=[byte[]]@(0x8B,0x44,0x24,0x04,0x56,0x8D,0x04,0x40) },
    [pscustomobject]@{ Name='original native Play CALL (signature-guarded)'; Va=0x00472870; Signature=[byte[]]@(0xE8,0xBB,0x19,0x09,0) }
)
$lines = New-Object 'System.Collections.Generic.List[string]'
[void]$lines.Add('SPIDER-MAN 2001 PC: NATIVE AUDIO LOADER ROOT-CAUSE RECONNAISSANCE')
[void]$lines.Add('STATIC / READ-ONLY; does not launch, inject, or patch the game')
[void]$lines.Add("Input: $resolvedExe")
[void]$lines.Add("SHA-256: $sha (VERIFIED)")
[void]$lines.Add("ImageBase: $(Format-HexVa $imageBase)")
[void]$lines.Add('')
[void]$lines.Add('VERIFIED ORIGINAL SIGNATURES AND KNOWN ADDRESSES')

foreach ($site in $known) {
    $count = if ($null -eq $site.Signature) { 16 } else { $site.Signature.Length }
    [byte[]]$actual = Get-BytesAtVa -Address $site.Va -Count $count
    if ($null -ne $site.Signature) {
        for ($j=0; $j -lt $count; $j++) {
            if ($actual[$j] -ne $site.Signature[$j]) {
                throw "Native signature mismatch: $($site.Name) at $(Format-HexVa $site.Va)"
            }
        }
    }
    $hexBytes = ([BitConverter]::ToString($actual)).Replace('-', ' ').ToLowerInvariant()
    $state = if ($null -eq $site.Signature) {
        '[address known; no instruction signature asserted]'
    } else {
        '[exact bytes MATCH]'
    }
    [void]$lines.Add("$($site.Name) @ $(Format-HexVa $site.Va): $hexBytes $state")
}

[void]$lines.Add('')
[void]$lines.Add('KNOWN / HYPOTHESIZED STAGES')
[void]$lines.Add('DXSOUND_Open @ 0x00504110 duplicates a reference buffer (known from prior game probes).')
[void]$lines.Add('Source loader DXSOUND_CreateDSBuffer, loadWAV, ParseWavHeader: ADDRESSES NOT YET VERIFIED.')
[void]$lines.Add('The 2048-byte file-sector boundary is a measured PCM symptom, not yet an identified instruction.')
[void]$lines.Add('Raw byte patterns and E8 targets below are CANDIDATES only, not confirmed instruction boundaries.')
[void]$lines.Add('')
[void]$lines.Add('RAW IMMEDIATE PATTERNS IN NATIVE AUDIO NEIGHBORHOOD')

$patterns = @(
    [pscustomobject]@{ Name='0x800 (2048 bytes)'; Bytes=[byte[]]@(0,8,0,0) },
    [pscustomobject]@{ Name='0x7FF (2047 mask)'; Bytes=[byte[]]@(255,7,0,0) },
    [pscustomobject]@{ Name='0xFFFFF800 (negative 2048)'; Bytes=[byte[]]@(0,248,255,255) },
    [pscustomobject]@{ Name='0x400 (1024 bytes)'; Bytes=[byte[]]@(0,4,0,0) },
    [pscustomobject]@{ Name='0x7FC (2044 bytes)'; Bytes=[byte[]]@(252,7,0,0) }
)
[byte[]]$windowBytes = Get-BytesAtVa -Address $windowStart -Count ([int]($windowEnd-$windowStart))
foreach ($pattern in $patterns) {
    $occurrences = New-Object 'System.Collections.Generic.List[string]'
    $total = 0
    for ($i=0; $i -le $windowBytes.Length - $pattern.Bytes.Length; $i++) {
        $equal = $true
        for ($j=0; $j -lt $pattern.Bytes.Length; $j++) {
            if ($windowBytes[$i+$j] -ne $pattern.Bytes[$j]) {
                $equal = $false
                break
            }
        }
        if ($equal) {
            $total++
            if ($occurrences.Count -lt $maxPatternAddresses) {
                [void]$occurrences.Add((Format-HexVa ($windowStart+$i)))
            }
        }
    }
    $addressText = if ($total -eq 0) { 'none' } else { $occurrences -join ', ' }
    if ($total -gt $maxPatternAddresses) {
        $addressText += ' [TRUNCATED]'
    }
    [void]$lines.Add("$($pattern.Name): $addressText")
}

[void]$lines.Add('')
[void]$lines.Add('POTENTIAL x86 CALL E8 REFERENCES INTO THIS AUDIO NEIGHBORHOOD')
$callCount = 0
$size = [int]$textSection.RawSize
$raw = [int]$textSection.RawOffset
for ($i = 0; $i -le $size - 5; $i++) {
    if ($exeBytes[$raw+$i] -ne 0xE8) { continue }
    $rel = [long]([BitConverter]::ToInt32($exeBytes, $raw+$i+1))
    $callerVa = [long]$textBaseVa + $i
    $target = $callerVa + 5 + $rel
    if ($target -lt 0) { $target += 4294967296L }
    if ($target -ge 4294967296L) { $target -= 4294967296L }
    if ($target -ge $windowStart -and $target -lt $windowEnd) {
        [void]$lines.Add("caller=$(Format-HexVa $callerVa) target=$(Format-HexVa $target)")
        $callCount++
        if ($callCount -ge $maxCandidateCalls) { break }
    }
}
if ($callCount -eq 0) {
    [void]$lines.Add('(no raw E8 candidates found)')
} elseif ($callCount -ge $maxCandidateCalls) {
    [void]$lines.Add('[CANDIDATES TRUNCATED]')
}

[void]$lines.Add('')
[void]$lines.Add('NATIVE CODE NEIGHBORHOOD / objdump disassembly')
[void]$lines.Add("VA $(Format-HexVa $windowStart)-$(Format-HexVa $windowEnd)")
[void]$lines.Add('Review function boundaries and COM indirect calls manually.')
[void]$lines.Add('Potential native DS interface vtable offsets on x86:')
[void]$lines.Add('  IDirectSound::CreateSoundBuffer +0x0C; DuplicateSoundBuffer +0x14')
[void]$lines.Add('  IDirectSoundBuffer::Lock +0x2C; Unlock +0x4C')
[void]$lines.Add('Offset appearance alone does NOT prove a DirectSound COM invocation.')
[void]$lines.Add('')

$disassembler = $null
if ($ObjdumpPath) {
    $disassembler = (Resolve-Path -LiteralPath $ObjdumpPath -ErrorAction Stop).ProviderPath
    if (-not [IO.File]::Exists($disassembler)) {
        throw "Not a disassembler executable: $disassembler"
    }
} else {
    foreach ($candidate in @('objdump.exe', 'llvm-objdump.exe')) {
        $found = Get-Command -Name $candidate -ErrorAction SilentlyContinue
        if ($found) {
            $disassembler = $found.Source
            break
        }
    }
}
if ($null -ne $disassembler) {
    [void]$lines.Add("Disassembler: $disassembler")
    # Second window follows calls from 0x00503B40 through the WAV
    # length + malloc + signed /2048 and suspected block reader.
    # Both windows stay static/read-only and share the EXE hash gate.
    $regions = @(
        [pscustomobject]@{ Label='native DirectSound loader / template fill'; Start=$windowStart; End=$windowEnd },
        [pscustomobject]@{ Label='WAV container open, length query and sector read callees'; Start=[long]0x00504C80; End=[long]0x00505300 }
    )
    foreach ($region in $regions) {
        [void]$lines.Add('')
        [void]$lines.Add("DISASSEMBLY: $($region.Label)")
        [void]$lines.Add("VA $(Format-HexVa $region.Start)-$(Format-HexVa $region.End)")
        try {
            # Passing one argument per token safely supports EXE paths with spaces.
            $startArg = ('--start-address=0x{0:X}' -f $region.Start)
            $stopArg  = ('--stop-address=0x{0:X}' -f $region.End)
            $output = & $disassembler -d -M intel $startArg $stopArg $resolvedExe 2>&1 | Out-String -Width 320
            if ($LASTEXITCODE -eq 0) {
                [void]$lines.Add($output)
            } else {
                [void]$lines.Add("Disassembler exit $LASTEXITCODE. Inspect tool installation.")
                [void]$lines.Add($output)
            }
        } catch {
            [void]$lines.Add("Disassembler failed: $($_.Exception.Message)")
        }
    }
} else {
    [void]$lines.Add('Disassembler unavailable: install GNU binutils or LLVM for Windows,')
    [void]$lines.Add('then run again (or pass -ObjdumpPath to objdump.exe / llvm-objdump.exe).')
    [void]$lines.Add('The SHA, PE, constants and CALL-candidate analysis above remains valid.')
}

$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
if ([string]::Equals($resolvedOutput, $resolvedExe, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to overwrite the input executable."
}
$outDir = [IO.Path]::GetDirectoryName($resolvedOutput)
if (-not [IO.Directory]::Exists($outDir)) {
    [void][IO.Directory]::CreateDirectory($outDir)
}
$report = [string]::Join([Environment]::NewLine, $lines) + [Environment]::NewLine
[IO.File]::WriteAllText($resolvedOutput, $report, [Text.UTF8Encoding]::new($false))
Write-Host "Original SpideyPC.exe verified (size and SHA-256)."
Write-Host "Report: $resolvedOutput"
if (-not $disassembler) {
    Write-Warning 'No disassembler found: install objdump or LLVM for complete assembly output.'
}
Write-Host 'No game file, process or archive was modified.'
