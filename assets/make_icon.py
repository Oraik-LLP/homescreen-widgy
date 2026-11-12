"""Generate the original app icon; no reference artwork or fonts are bundled."""
from PIL import Image, ImageDraw
from pathlib import Path

root = Path(__file__).parent
factor = 4
im = Image.new('RGBA', (256*factor,256*factor))
d = ImageDraw.Draw(im)
def panel(box, radius, fill):
    d.rounded_rectangle(tuple(v*factor for v in box), radius=radius*factor, fill=fill)
panel((8,8,248,248), 56, '#213A30')
panel((50,50,123,123), 13, '#F2F4E8')
panel((134,50,207,123), 13, '#B9D3B8')
panel((50,134,123,207), 13, '#91B89D')
panel((134,134,207,207), 13, '#D6D5A3')
im.save(root / 'panes-icon-1024.png')
im = im.resize((256,256), Image.Resampling.LANCZOS)
im.save(root / 'app.ico', sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
im.save(root / 'app.png')
