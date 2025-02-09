(function() {
    class Color {
        constructor(r, g, b) {
            this.r = r;
            this.g = g;
            this.b = b;
        }

        brightness(val) {
            return new Color(this.r * val, this.g * val, this.b * val);
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
            this.color = blockColors[Math.floor(Math.random() * blockColors.length)].brightness(Math.random() * 0.2 + 0.8);
        }
    }

    class BlockRow {
        blocks = [];       
        width = 0;
        y = 0;
        speed = 2;

        constructor(y, speed) {
            this.y = y;
            this.speed = speed;
            let x = leftBound;
            let lastW = this.addBlock(leftBound);
            while (x + lastW + blockGap < rightBound) {
                x += lastW + blockGap;
                lastW = this.addBlock(x);
            }
        }

        addBlock(x) {
            this.blocks.push(new Block(x, this.y));
            const w = this.blocks[this.blocks.length - 1].w;
            this.width += w + blockGap;
            return w;
        }

        advance() {
            for (const block of this.blocks) {
                block.x -= this.speed;
            }
            if (this.blocks[0].x + this.blocks[0].w < leftBound) {
                let v = this.blocks.shift();
                this.width -= v.w + blockGap;
            }
            if (this.blocks[0].x + this.width < rightBound) {
                this.addBlock(this.blocks[0].x + this.width);
            }
        }

        draw(ctx) {
            ctx.lineWidth = 2;
            for (const block of this.blocks) {
                ctx.strokeStyle = block.color.brightness(0.75).toString();
                ctx.fillStyle = block.color.toString();
                ctx.fillRect(block.x, block.y, block.w, block.h);
                ctx.strokeRect(block.x, block.y, block.w, block.h);
            }
        }
    }

    const blockGap = 25;

    const blockColors = [
        new Color(0xff, 0x99, 0x00),
        new Color(0x00, 0xaa, 0x44),
        new Color(0x00, 0xcc, 0x77),
        new Color(0x77, 0xcc, 0x44),
        new Color(0xff, 0x00, 0x99),
        new Color(0xff, 0x77, 0x00),
        new Color(0xff, 0x44, 0x00),
        new Color(0x77, 0x77, 0x77),
        new Color(0x99, 0x00, 0xff),
    ];

    /**
        * @type HTMLCanvasElement
    */
    const canv = document.getElementById('title-bg');
    if (!canv) {
        console.log('No canvas :(');
        return;
    }

    /**
        * @type CanvasRenderingContext2D
    */
    const ctx = canv.getContext('2d');
    if (!ctx) {
        console.log('No context :(');
        return;
    }

    canv.width = window.innerWidth;
    canv.height = window.innerHeight;
    window.addEventListener('resize', function(_) {
        canv.width = window.innerWidth;
        canv.height = window.innerHeight;
        leftBound   = canv.width  * -0.25;
        rightBound  = canv.width  *  1.25;
        topBound    = canv.height * -0.25;
        bottomBound = canv.height *  1.3;
    });

    let leftBound   = canv.width  * -0.25;
    let rightBound  = canv.width  *  1.25;
    let topBound    = canv.height * -0.25;
    let bottomBound = canv.height *  1.3;

    let rows = [];
    let y = topBound;
    while (y + 32 + blockGap < bottomBound) {
        rows.push(new BlockRow(y, Math.random() * 1.5 + 0.4));
        y += 32 + blockGap;
    }

    function drawBlocks() {
        ctx.clearRect(0, 0, canv.width, canv.height);
        ctx.save();
        ctx.translate(canv.width / 2, canv.height / 2);
        ctx.rotate(-15 / 180 * Math.PI);
        ctx.translate(-canv.width / 2, -canv.height / 2);
        for (const row of rows) {
            row.advance();
            row.draw(ctx);
        }
        ctx.restore();

        ctx.fillStyle = '#0008';
        ctx.fillRect(0, 0, canv.width, canv.height);
        requestAnimationFrame(drawBlocks);
    }

    drawBlocks();
})();
