"""Compare mild spatial smoothing against the 64-color second batch."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools' / 'gif-processing'))
import imageio_ffmpeg
from PIL import Image, ImageDraw

OUT = ROOT / 'gifs' / 'otimizados' / 'lote-v3'
OUT.mkdir(parents=True, exist_ok=True)


def inspect(path):
    with Image.open(path) as im:
        durations, disposals = [], set()
        for n in range(im.n_frames):
            im.seek(n)
            im.load()
            assert im.convert('RGBA').getextrema()[3] == (255, 255)
            durations.append(im.info.get('duration', 0))
            disposals.add(im.disposal_method)
        assert im.size == (320, 480)
        assert disposals <= {0, 1, 2}
        return dict(file=str(path.relative_to(ROOT)), bytes=path.stat().st_size,
                    frames=im.n_frames, duration_ms=sum(durations),
                    size=list(im.size), disposals=sorted(disposals))


reports = []
for number in range(1, 5):
    source = ROOT / 'gifs' / f'video{number}.gif'
    baseline = ROOT / 'gifs' / 'otimizados' / 'lote-v2' / '64cores' / source.name
    reference = inspect(baseline)
    paths = [baseline]
    for label, sigma in [('leve', 0.6), ('moderada', 1.0)]:
        folder = OUT / label
        folder.mkdir(exist_ok=True)
        target = folder / source.name
        filters = (f'[0:v]fps=15,format=rgb24,gblur=sigma={sigma},split[a][b];'
                   '[a]palettegen=max_colors=64:reserve_transparent=0[p];'
                   '[b][p]paletteuse=dither=none[v]')
        subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-hide_banner', '-loglevel',
                        'error', '-y', '-i', str(source), '-filter_complex', filters,
                        '-map', '[v]', '-an', '-gifflags', '-transdiff', '-loop', '0',
                        str(target)], check=True)
        result = inspect(target)
        assert result['frames'] == reference['frames']
        assert result['duration_ms'] == reference['duration_ms']
        result['reduction_vs_v2_percent'] = round(100 * (1 - result['bytes'] / reference['bytes']), 1)
        result['sigma'] = sigma
        reports.append(result)
        paths.append(target)
        print(json.dumps(result), flush=True)
    # Native-resolution mid-frame previews preserve fine lettering for review.
    sheet = Image.new('RGB', (960, 510), 'white')
    draw = ImageDraw.Draw(sheet)
    for col, (label, path) in enumerate(zip(('Lote v2', 'Suavizacao leve (0.6)', 'Suavizacao moderada (1.0)'), paths)):
        draw.text((col * 320 + 4, 6), label, fill='black')
        with Image.open(path) as im:
            im.seek(im.n_frames // 2)
            sheet.paste(im.convert('RGB'), (col * 320, 30))
    sheet.save(OUT / f'comparacao-video{number}.png')
(OUT / 'analise.json').write_text(json.dumps(reports, indent=2), encoding='utf-8')
