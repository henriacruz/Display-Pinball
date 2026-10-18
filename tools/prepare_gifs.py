"""Prepare opaque, low-entropy GIF candidates without changing originals."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools' / 'gif-processing'))
import imageio_ffmpeg
from PIL import Image, ImageDraw

OUT = ROOT / 'gifs' / 'otimizados' / 'lote-v2'
OUT.mkdir(parents=True, exist_ok=True)


def inspect(path):
    with Image.open(path) as im:
        durations = []
        disposals = set()
        opaque = True
        for index in range(im.n_frames):
            im.seek(index)
            im.load()
            durations.append(im.info.get('duration', 0))
            disposals.add(im.disposal_method)
            opaque &= im.convert('RGBA').getextrema()[3] == (255, 255)
        return dict(file=str(path.relative_to(ROOT)), bytes=path.stat().st_size,
                    size=list(im.size), frames=im.n_frames, duration_ms=sum(durations),
                    disposals=sorted(disposals), opaque=opaque)


reports = []
for number in range(1, 5):
    source = ROOT / 'gifs' / f'video{number}.gif'
    original = inspect(source)
    candidates = [source]
    for colors in (128, 64):
        folder = OUT / f'{colors}cores'
        folder.mkdir(exist_ok=True)
        target = folder / source.name
        filters = (f'[0:v]fps=15,split[a][b];'
                   f'[a]palettegen=max_colors={colors}:reserve_transparent=0[p];'
                   '[b][p]paletteuse=dither=none[v]')
        subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-hide_banner', '-loglevel',
                        'error', '-y', '-i', str(source), '-filter_complex', filters,
                        '-map', '[v]', '-an', '-gifflags', '-transdiff', '-loop', '0',
                        str(target)], check=True)
        result = inspect(target)
        assert result['size'] == original['size'] == [320, 480]
        assert abs(result['duration_ms'] - original['duration_ms']) <= 100
        assert set(result['disposals']) <= {0, 1, 2}
        assert result['opaque']
        result['reduction_percent'] = round(100 * (1 - result['bytes'] / original['bytes']), 1)
        reports.append(result)
        candidates.append(target)
        print(json.dumps(result), flush=True)

    sheet = Image.new('RGB', (640, 3 * 350), 'white')
    draw = ImageDraw.Draw(sheet)
    for row, path in enumerate(candidates):
        label = 'Original' if row == 0 else f'{(128, 64)[row-1]} cores / 15 fps / sem dithering'
        draw.text((8, row * 350 + 5), label, fill='black')
        with Image.open(path) as im:
            for col, n in enumerate((0, im.n_frames // 2, im.n_frames - 1)):
                im.seek(n)
                sheet.paste(im.convert('RGB').resize((200, 300)), (col * 212, row * 350 + 30))
    sheet.save(OUT / f'comparacao-video{number}.png')

(OUT / 'analise.json').write_text(json.dumps(reports, indent=2), encoding='utf-8')
