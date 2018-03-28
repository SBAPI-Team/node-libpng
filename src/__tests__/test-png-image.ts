import { readFileSync } from "fs";
import { PngImage } from "../png-image";
import { ColorType } from "../color-type";
import { xy } from "../xy";
import { rect } from "../rect";
import {
    isColorRGB,
    isColorRGBA,
    isColorPalette,
    isColorGrayScale,
    isColorGrayScaleAlpha,
    colorRGB,
    colorRGBA,
} from "../colors";
import { expectRedBlueGradient } from "./utils";

describe("PngImage", () => {
    it("reads the info of a normal png file", () => {
        const buffer = readFileSync(`${__dirname}/fixtures/red-blue-gradient-256px.png`);
        const image = new PngImage(buffer);

        expect(image.bitDepth).toBe(8);
        expect(image.channels).toBe(3);
        expect(image.colorType).toBe("rgb");
        expect(image.width).toBe(256);
        expect(image.height).toBe(256);
        expect(image.interlaceType).toBe("none");
        expect(image.rowBytes).toBe(256 * 3);
        expect(image.offsetX).toBe(0);
        expect(image.offsetY).toBe(0);
        expect(image.pixelsPerMeterX).toBe(0);
        expect(image.pixelsPerMeterY).toBe(0);
        expect(image.palette).toBeUndefined();
        expect(image.backgroundColor).toBeUndefined();
        expect(image.bytesPerPixel).toBe(3);
        expect(image.gamma).toBeUndefined();
        expect(image.time).toBeUndefined();
    });

    it("decodes a normal png file", () => {
        const inputBuffer = readFileSync(`${__dirname}/fixtures/red-blue-gradient-256px.png`);
        const { data } = new PngImage(inputBuffer);
        expectRedBlueGradient(data);
    });

    it("decodes a PNG file with ADAM7 interlacing", () => {
        const inputBuffer = readFileSync(`${__dirname}/fixtures/red-blue-gradient-256px-interlaced.png`);
        const { data } = new PngImage(inputBuffer);
        expectRedBlueGradient(data);
    });

    it("reads the palette of a ColorType.PALETTE image", () => {
        const inputBuffer = readFileSync(`${__dirname}/fixtures/indexed-16px.png`);
        const { palette } = new PngImage(inputBuffer);
        expect(palette.size).toBe(5);
        expect(Array.from(palette.entries())).toEqual([
            [0, [0, 0, 0]],
            [1, [128, 255, 64]],
            [2, [64, 128, 255]],
            [3, [255, 64, 128]],
            [4, [255, 128, 64]],
        ]);
    });

    it("reads the background color of an image with the background color specified", () => {
        const inputBuffer = readFileSync(`${__dirname}/fixtures/background-color.png`);
        const { backgroundColor } = new PngImage(inputBuffer);
        expect(backgroundColor).toEqual([255, 128, 64]);
    });

    it("reads the time of an image with the time specified in the header", () => {
        const inputBuffer = readFileSync(`${__dirname}/fixtures/orange-rectangle-time.png`);
        const { time } = new PngImage(inputBuffer);
        expect(time).toEqual(new Date("2018-04-26T05:28:16.000Z"));
    });

    it("reads the gamma value of an image with the gamma value specified in the header", () => {
        const inputBuffer = readFileSync(`${__dirname}/fixtures/orange-rectangle-gamma-background.png`);
        const { gamma } = new PngImage(inputBuffer);
        expect(gamma).toEqual(0.45455);
    });

    it("converts XY coordinates to Index", () => {
        const inputBuffer = readFileSync(`${__dirname}/fixtures/orange-rectangle.png`);
        const image = new PngImage(inputBuffer);
        expect(image.width).toBe(32);
        expect(image.height).toBe(16);
        expect(image.bytesPerPixel).toBe(3);
        expect(image.toXY(5)).toEqual([1, 0]);
        expect(image.toXY(32 * 3 * 10 + 3 * 5)).toEqual([5, 10]);
        expect(image.toXY(image.data.length - 2)).toEqual([31, 15]);
    });

    it("converts Index to XY coordinates", () => {
        const inputBuffer = readFileSync(`${__dirname}/fixtures/orange-rectangle.png`);
        const image = new PngImage(inputBuffer);
        expect(image.width).toBe(32);
        expect(image.height).toBe(16);
        expect(image.bytesPerPixel).toBe(3);
        expect(image.toIndex(1, 0)).toEqual(3);
        expect(image.toIndex(5, 10)).toEqual(32 * 3 * 10 + 3 * 5);
        expect(image.toIndex(31, 15)).toEqual(image.data.length - 3);
    });

    it("converts Index to XY coordinates", () => {
        const inputBuffer = readFileSync(`${__dirname}/fixtures/orange-rectangle.png`);
        const image = new PngImage(inputBuffer);
        expect(image.width).toBe(32);
        expect(image.height).toBe(16);
        expect(image.bytesPerPixel).toBe(3);
        expect(image.toIndex(1, 0)).toEqual(3);
        expect(image.toIndex(5, 10)).toEqual(32 * 3 * 10 + 3 * 5);
        expect(image.toIndex(31, 15)).toEqual(image.data.length - 3);
    });

    [
        {
            colorType: ColorType.GRAY_SCALE,
            file: `${__dirname}/fixtures/grayscale-gradient-16px.png`,
            checker: isColorGrayScale,
            rgbaColor: [85, 85, 85, 255],
        },
        {
            colorType: ColorType.GRAY_SCALE_ALPHA,
            file: `${__dirname}/fixtures/grayscale-alpha-gradient-16px.png`,
            checker: isColorGrayScaleAlpha,
            rgbaColor: [85, 85, 85, 127],
        },
        {
            colorType: ColorType.RGB,
            file: `${__dirname}/fixtures/orange-rectangle.png`,
            checker: isColorRGB,
            rgbaColor: [255, 128, 64, 255],
        },
        {
            colorType: ColorType.RGBA,
            file: `${__dirname}/fixtures/opaque-rectangle.png`,
            checker: isColorRGBA,
            rgbaColor: [255, 128, 64, 127],
        },
        {
            colorType: ColorType.PALETTE,
            file: `${__dirname}/fixtures/indexed-16px.png`,
            checker: isColorPalette,
            rgbaColor: [255, 64, 128, 255],
        },
    ].forEach(({ colorType, file, checker, rgbaColor }) => {
        it(`detects the correct color with an image of color type ${colorType}`, () => {
            const inputBuffer = readFileSync(file);
            const image = new PngImage(inputBuffer);
            expect(image.colorType).toBe(colorType);
            const color = image.at(10, 10);
            expect(checker(color)).toBe(true);
            expect(color).toMatchSnapshot();
        });

        it(`reads the correct in rgba format with an image of color type ${colorType}`, () => {
            const inputBuffer = readFileSync(file);
            const image = new PngImage(inputBuffer);
            expect(image.colorType).toBe(colorType);
            const color = image.rgbaAt(10, 10);
            expect(color).toEqual(rgbaColor);
        });
    });

    describe("export methods", () => {
        const somePngImage = new PngImage(readFileSync(`${__dirname}/fixtures/orange-rectangle.png`));

        describe("encode", () => {
            it("encodes a PNG file with the colortype being RGB", () => {
                expect(somePngImage.encode().toString("hex")).toMatchSnapshot();
            });
        });

        describe("write", () => {
            it("writes a PNG file with the colortype being RGB", async () => {
                const path = `${__dirname}/../../tmp-png-image-write.png`;
                await somePngImage.write(path);
                const fromDisk = readFileSync(path);
                expect(fromDisk.toString("hex")).toMatchSnapshot();
            });
        });

        describe("writeSync", () => {
            it("writes a PNG file with the colortype being RGB", () => {
                const path = `${__dirname}/../../tmp-png-image-write-sync.png`;
                somePngImage.writeSync(path);
                const fromDisk = readFileSync(path);
                expect(fromDisk.toString("hex")).toMatchSnapshot();
            });
        });
    });

    describe("resizing the canvas", () => {
        it("resizes the canvas of a simple RGB image", () => {
            const somePngImage = new PngImage(readFileSync(`${__dirname}/fixtures/orange-rectangle.png`));
            somePngImage.resizeCanvas(xy(18, 18), xy(10, 10), rect(0, 0, 6, 6), colorRGB(0, 0, 128));
            somePngImage.writeSync(`${__dirname}/../../tmp-png-image-resize-simple-rgb.png`);
        });

        it("resizes the canvas of a bigger RGB image", () => {
            const somePngImage = new PngImage(readFileSync(`${__dirname}/fixtures/red-blue-gradient-256px.png`));
            somePngImage.resizeCanvas(
                xy(300, 100),
                xy(22, 10),
                rect(0, 0, 256, 80),
                colorRGB(255, 255, 200),
            );
            somePngImage.writeSync(`${__dirname}/../../tmp-png-image-resize-bigger-rgb.png`);
        });

        it("resizes the canvas to a huge region", () => {
            const somePngImage = new PngImage(readFileSync(`${__dirname}/fixtures/opaque-rectangle.png`));
            somePngImage.resizeCanvas(
                xy(400, 400),
                xy(368, 192),
                rect(0, 0, 32, 16),
                colorRGBA(128, 128, 128, 128),
            );
            somePngImage.writeSync(`${__dirname}/../../tmp-png-image-resize-alpha-huge-region.png`);
        });
    });
});
