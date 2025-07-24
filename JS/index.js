(function () {
  class Color {
    constructor(r, g, b) {
      this.r = r;
      this.g = g;
      this.b = b;
    }

    brightness(factor) {
      return new Color(this.r * factor, this.g * factor, this.b * factor);
    }

    toString() {
      return `rgb(${this.r}, ${this.g}, ${this.b})`;
    }
  }

  class Block {
    constructor(x, y) {
      this.x = x;
      this.y = y;
      this.w = Math.random() * 200 + 40;
      this.h = 32;
      const baseColor = blockColors[Math.floor(Math.random() * blockColors.length)];
      this.color = baseColor.brightness(Math.random() * 0.2 + 0.8);
    }
  }

  class BlockRow {
    blocks = [];
    width = 0;

    constructor(y, speed) {
      this.y = y;
      this.speed = speed;
      let x = leftBound;
      let lastWidth = this.addBlock(x);
      while (x + lastWidth + blockGap < rightBound) {
        x += lastWidth + blockGap;
        lastWidth = this.addBlock(x);
      }
    }

    addBlock(x) {
      const block = new Block(x, this.y);
      this.blocks.push(block);
      const width = block.w;
      this.width += width + blockGap;
      return width;
    }

    advance() {
      for (const block of this.blocks) {
        block.x -= this.speed;
      }

      if (this.blocks[0].x + this.blocks[0].w < leftBound) {
        const removed = this.blocks.shift();
        this.width -= removed.w + blockGap;
      }

      const lastX = this.blocks[0].x + this.width;
      if (lastX < rightBound) {
        this.addBlock(this.blocks[0].x + this.width);
      }
    }

    draw(ctx) {
      ctx.lineWidth = 2;
      for (const block of this.blocks) {
        ctx.fillStyle = block.color.toString();
        ctx.strokeStyle = block.color.brightness(0.75).toString();
        ctx.fillRect(block.x, block.y, block.w, block.h);
        ctx.strokeRect(block.x, block.y, block.w, block.h);
      }
    }
  }

  const blockGap = 25;
  const blockColors = [
    new Color(255, 153, 0),
    new Color(0, 170, 68),
    new Color(0, 204, 119),
    new Color(119, 204, 68),
    new Color(255, 0, 153),
    new Color(255, 119, 0),
    new Color(255, 68, 0),
    new Color(119, 119, 119),
    new Color(153, 0, 255),
  ];

  const canvas = document.querySelector('.title-bg');
  const ctx = canvas.getContext('2d');
  if (!ctx) return console.error('Canvas context missing.');

  const updateCanvasSize = () => {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;

    leftBound = canvas.width * -0.25;
    rightBound = canvas.width * 1.25;
    topBound = canvas.height * -0.25;
    bottomBound = canvas.height * 1.3;
  };

  let leftBound, rightBound, topBound, bottomBound;
  updateCanvasSize();
  window.addEventListener('resize', updateCanvasSize);

  const rows = [];
  let y = topBound;
  while (y + 32 + blockGap < bottomBound) {
    rows.push(new BlockRow(y, Math.random() * 1.5 + 0.4));
    y += 32 + blockGap;
  }

  function drawBlocks() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.save();
    ctx.translate(canvas.width / 2, canvas.height / 2);
    ctx.rotate((-15 * Math.PI) / 180);
    ctx.translate(-canvas.width / 2, -canvas.height / 2);

    for (const row of rows) {
      row.advance();
      row.draw(ctx);
    }

    ctx.restore();

    ctx.fillStyle = '#0008';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    requestAnimationFrame(drawBlocks);
  }

  drawBlocks();
})();
