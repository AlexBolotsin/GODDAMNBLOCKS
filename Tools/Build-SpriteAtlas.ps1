param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [int]$MinPixels = 200,
    [int]$MaxContentY = 349,
    [int]$RowMergeTolerance = 6
)

Add-Type -AssemblyName System.Drawing

$bitmap = [System.Drawing.Bitmap]::FromFile($InputPath)
try {
    $width = $bitmap.Width
    $height = $bitmap.Height
    $bg = $bitmap.GetPixel(0, 0)
    $bgArgb = $bg.ToArgb()

    $visited = New-Object 'bool[,]' $width, $height
    $directions = @(
        @(1, 0),
        @(-1, 0),
        @(0, 1),
        @(0, -1)
    )

    $components = New-Object System.Collections.Generic.List[object]

    for ($y = 0; $y -lt $height; $y++) {
        for ($x = 0; $x -lt $width; $x++) {
            if ($visited[$x, $y]) {
                continue
            }

            $visited[$x, $y] = $true
            if ($bitmap.GetPixel($x, $y).ToArgb() -eq $bgArgb) {
                continue
            }

            $queue = New-Object System.Collections.Generic.Queue[object]
            $queue.Enqueue(@($x, $y))

            $minX = $x
            $minY = $y
            $maxX = $x
            $maxY = $y
            $pixelCount = 0

            while ($queue.Count -gt 0) {
                $point = $queue.Dequeue()
                $px = [int]$point[0]
                $py = [int]$point[1]
                $pixelCount++

                if ($px -lt $minX) { $minX = $px }
                if ($py -lt $minY) { $minY = $py }
                if ($px -gt $maxX) { $maxX = $px }
                if ($py -gt $maxY) { $maxY = $py }

                foreach ($dir in $directions) {
                    $nx = $px + $dir[0]
                    $ny = $py + $dir[1]

                    if ($nx -lt 0 -or $ny -lt 0 -or $nx -ge $width -or $ny -ge $height) {
                        continue
                    }

                    if ($visited[$nx, $ny]) {
                        continue
                    }

                    $visited[$nx, $ny] = $true
                    if ($bitmap.GetPixel($nx, $ny).ToArgb() -ne $bgArgb) {
                        $queue.Enqueue(@($nx, $ny))
                    }
                }
            }

            if ($pixelCount -ge $MinPixels -and $maxY -le $MaxContentY) {
                $components.Add([pscustomobject]@{
                    x = $minX
                    y = $minY
                    width = $maxX - $minX + 1
                    height = $maxY - $minY + 1
                    pixels = $pixelCount
                }) | Out-Null
            }
        }
    }

    $sortedComponents = $components | Sort-Object y, x
    $rows = New-Object System.Collections.Generic.List[object]

    foreach ($component in $sortedComponents) {
        $row = $null
        foreach ($candidate in $rows) {
            if ([Math]::Abs($candidate.y - $component.y) -le $RowMergeTolerance) {
                $row = $candidate
                break
            }
        }

        if ($null -eq $row) {
            $row = [pscustomobject]@{
                rowIndex = $rows.Count
                y = $component.y
                frames = New-Object System.Collections.Generic.List[object]
            }
            $rows.Add($row) | Out-Null
        }

        $frameIndex = $row.frames.Count
        $frameName = ('frame_{0:D3}' -f ($sortedComponents.IndexOf($component)))
        $row.frames.Add([pscustomobject]@{
            name = $frameName
            row = $row.rowIndex
            indexInRow = $frameIndex
            x = $component.x
            y = $component.y
            width = $component.width
            height = $component.height
            pixels = $component.pixels
        }) | Out-Null
    }

    $frames = @()
    $clips = @()
    foreach ($row in $rows) {
        $orderedFrames = $row.frames | Sort-Object x
        $rowNames = @()
        foreach ($frame in $orderedFrames) {
            $frames += $frame
            $rowNames += $frame.name
        }

        $clips += [pscustomobject]@{
            name = ('row_{0:D2}' -f $row.rowIndex)
            row = $row.rowIndex
            y = $row.y
            frameCount = $orderedFrames.Count
            frames = $rowNames
        }
    }

    $atlas = [pscustomobject]@{
        sourceImage = [System.IO.Path]::GetFileName($InputPath)
        imageWidth = $width
        imageHeight = $height
        chromaKey = [pscustomobject]@{
            r = $bg.R
            g = $bg.G
            b = $bg.B
        }
        contentBounds = [pscustomobject]@{
            x = 0
            y = 7
            width = $width
            height = 343
        }
        extraction = [pscustomobject]@{
            mode = 'connected-components'
            minPixels = $MinPixels
            maxContentY = $MaxContentY
            rowMergeTolerance = $RowMergeTolerance
        }
        frames = $frames
        clips = $clips
    }

    $outputDir = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($outputDir) -and -not (Test-Path $outputDir)) {
        New-Item -ItemType Directory -Path $outputDir | Out-Null
    }

    $atlas | ConvertTo-Json -Depth 6 | Set-Content -Path $OutputPath -Encoding UTF8
}
finally {
    $bitmap.Dispose()
}
