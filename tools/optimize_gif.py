"""Create the video2 comparison using locally installed FFmpeg and Pillow."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools' / 'gif-processing'))
import imageio_ffmpeg
from PIL import Image, ImageDraw

out = ROOT / 'gifs' / 'otimizados'
out.mkdir(parents=True, exist_ok=True)
source = ROOT / 'gifs' / 'video2.gif'
reports = []
paths = [source]
for colors in (256, 128):
    target = out / f'video2_15fps_{colors}cores.gif'
    filters = (f'[0:v]fps=15,split[a][b];'
               f'[a]palettegen=max_colors={colors}:reserve_transparent=0[p];'
               '[b][p]paletteuse=dither=bayer:bayer_scale=3[v]')
    subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-hide_banner', '-loglevel',
                    'error', '-y', '-i', str(source), '-filter_complex', filters,
                    '-map', '[v]', '-an', '-gifflags', '-transdiff', '-loop', '0',
                    str(target)], check=True)
    paths.append(target)

sheet = Image.new('RGB', (640, 3 * 350), 'white')
draw = ImageDraw.Draw(sheet)
for row, path in enumerate(paths):
    with Image.open(path) as im:
        delays, disposals = [], set()
        first_opaque = False
        for n in range(im.n_frames):
            im.seek(n)
            if n == 0:
                first_opaque = im.convert('RGBA').getextrema()[3] == (255, 255)
            delays.append(im.info.get('duration', 0))
            disposals.add(im.disposal_method)
            im.load()  # Decode every frame to detect damaged output.
        assert im.size == (320, 480)
        assert disposals <= {0, 1, 2}, disposals
        if path != source:
            assert first_opaque
            assert abs(sum(delays) - 5000) <= 100
        reports.append(dict(file=str(path.relative_to(ROOT)), bytes=path.stat().st_size,
                            size=list(im.size), frames=im.n_frames,
                            duration_ms=sum(delays), disposals=sorted(disposals),
                            first_frame_opaque=first_opaque))
        draw.text((8, row * 350 + 5), path.name, fill='black')
        for col, n in enumerate((0, im.n_frames // 2, im.n_frames - 1)):
            im.seek(n)
            preview = im.convert('RGB').resize((200, 300))
            sheet.paste(preview, (col * 212, row * 350 + 30))
sheet.save(out / 'comparacao-video2.png')
(out / 'analise-video2.json').write_text(json.dumps(reports, indent=2), encoding='utf-8')
print(json.dumps(reports, indent=2))
