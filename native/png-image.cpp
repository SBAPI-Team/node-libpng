#include "png-image.hpp"

#include <node_buffer.h>
#include <string>
#include <vector>
#include <iostream>

using namespace node;
using namespace v8;
using namespace std;

Nan::Persistent<Function> constructor;

PngImage::PngImage(png_structp &pngPtr, png_infop &infoPtr, uint32_t inputSize, uint8_t *input) :
    pngPtr(pngPtr),
    infoPtr(infoPtr),
    // The amount of bytes in the input buffer.
    inputSize(inputSize),
    // Store the input buffer.
    input(input),
    // The header of the PNG file is 8 bytes long and needs to be skipped, hence initialize the `consumed` counter
    // with 8.
    consumed(8),
    decoded(nullptr),
    decodedSize(0) {}

PngImage::~PngImage() {}

NAN_MODULE_INIT(PngImage::Init) {
    Nan::HandleScope scope;
    // Define the constructor. Rename `PngImage` to `NativePngImage` as it will be wrapped on JS side.
    auto ctor = Nan::New<FunctionTemplate>(PngImage::New);
    auto ctorInstance = ctor->InstanceTemplate();
    ctor->SetClassName(Nan::New("__native_PngImage").ToLocalChecked());
    ctorInstance->SetInternalFieldCount(5);
    // Hand over all getters defined on `PngImage` to node.
    Nan::SetAccessor(ctorInstance, Nan::New("bitDepth").ToLocalChecked(), PngImage::getBitDepth);
    Nan::SetAccessor(ctorInstance, Nan::New("channels").ToLocalChecked(), PngImage::getChannels);
    Nan::SetAccessor(ctorInstance, Nan::New("colorType").ToLocalChecked(), PngImage::getColorType);
    Nan::SetAccessor(ctorInstance, Nan::New("height").ToLocalChecked(), PngImage::getHeight);
    Nan::SetAccessor(ctorInstance, Nan::New("width").ToLocalChecked(), PngImage::getWidth);
    Nan::SetAccessor(ctorInstance, Nan::New("interlaceType").ToLocalChecked(), PngImage::getInterlaceType);
    Nan::SetAccessor(ctorInstance, Nan::New("rowBytes").ToLocalChecked(), PngImage::getRowBytes);
    Nan::SetAccessor(ctorInstance, Nan::New("offsetX").ToLocalChecked(), PngImage::getOffsetX);
    Nan::SetAccessor(ctorInstance, Nan::New("offsetY").ToLocalChecked(), PngImage::getOffsetY);
    Nan::SetAccessor(ctorInstance, Nan::New("pixelsPerMeterX").ToLocalChecked(), PngImage::getPixelsPerMeterX);
    Nan::SetAccessor(ctorInstance, Nan::New("pixelsPerMeterY").ToLocalChecked(), PngImage::getPixelsPerMeterY);
    Nan::SetAccessor(ctorInstance, Nan::New("buffer").ToLocalChecked(), PngImage::getBuffer);
    // Make sure the constructor stays persisted by storing it in a `Nan::Persistant`.
    constructor.Reset(ctor->GetFunction());
    // Store `NativePngImage` in the module's exports.
    Nan::Set(target, Nan::New("__native_PngImage").ToLocalChecked(), ctor->GetFunction());
}

NAN_METHOD(PngImage::New) {
    if (info.IsConstructCall()) {
        // 1st Parameter: The input buffer.
        Local<Object> inputBuffer = Local<Object>::Cast(info[0]);
        uint32_t inputSize = Buffer::Length(inputBuffer);
        uint8_t *input = reinterpret_cast<uint8_t*>(Buffer::Data(inputBuffer));

        // Check if the buffer contains a PNG image at all.
        if (inputSize < 8 || png_sig_cmp(input, 0, 8)) {
            Nan::ThrowTypeError("Invalid PNG buffer.");
            return;
        }

        auto pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!pngPtr) {
            Nan::ThrowTypeError("Could not create PNG read struct.");
            return;
        }
        // Try to grab the info struct from the loaded PNG file.
        auto infoPtr = png_create_info_struct(pngPtr);
        if (!infoPtr) {
            Nan::ThrowTypeError("Could not create PNG info struct.");
            return;
        }
        // libpng will jump to this if an error occured while reading.
        if (setjmp(png_jmpbuf(pngPtr))) {
            Nan::ThrowTypeError("Error decoding PNG buffer.");
            return;
        }
        // Create instance of `PngImage`.
        PngImage* instance = new PngImage(pngPtr, infoPtr, inputSize, input);
        instance->Wrap(info.This());
        // This callback will be called each time libpng requests a new chunk.
        png_set_read_fn(pngPtr, reinterpret_cast<png_voidp>(instance), [] (png_structp passedStruct, png_bytep target, png_size_t length) {
            auto pngImage = reinterpret_cast<PngImage*>(png_get_io_ptr(passedStruct));
            memcpy(reinterpret_cast<uint8_t*>(target), pngImage->input + pngImage->consumed, length);
            pngImage->consumed += length;
        });
        // Tell libpng that the initial 8 bytes for the header have already been read.
        png_set_sig_bytes(pngPtr, 8);
        // Read the infos.
        png_read_info(pngPtr, infoPtr);

        auto rowCount = png_get_image_height(pngPtr, infoPtr);
        auto rowBytes = png_get_rowbytes(pngPtr, infoPtr);
        instance->decodedSize = rowBytes * rowCount;
        instance->decoded = reinterpret_cast<uint8_t*>(malloc(instance->decodedSize));

        instance->rows.resize(rowCount, nullptr);
        instance->decoded = new png_byte[instance->decodedSize];
        for(size_t row = 0; row < rowCount; ++row) {
            instance->rows[row] = instance->decoded + row * rowBytes;
        }
        png_read_image(pngPtr, &instance->rows[0]);

        // Set the return value of the call to the constructor to the newly created instance.
        info.GetReturnValue().Set(info.This());
    } else {
        // Invoked as plain function `PngImage(...)`, turn into constructor call.
        vector<Local<Value>> args(info.Length());
        for (size_t i = 0; i < args.size(); ++i) {
            args[i] = info[i];
        }
        auto instance = Nan::NewInstance(info.Callee(), args.size(), args.data());
        if (!instance.IsEmpty()) {
            info.GetReturnValue().Set(instance.ToLocalChecked());
        }
    }
}

/**
 * This getter will return the width of the image, gathered from `png_get_image_width`.
 */
NAN_GETTER(PngImage::getWidth) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.This());
    double width = png_get_image_width(pngImageInstance->pngPtr, pngImageInstance->infoPtr);
    // libpng will return `0` instead of the width if it couldn't be read.
    if (width == 0) {
        Nan::ThrowError("Unable to read width from PNG.");
    }
    info.GetReturnValue().Set(Nan::New(width));
}

/**
 * This getter will return the width of the image, gathered from `png_get_image_height`.
 */
NAN_GETTER(PngImage::getHeight) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.This());
    double height = png_get_image_height(pngImageInstance->pngPtr, pngImageInstance->infoPtr);
    // libpng will return `0` instead of the height if it couldn't be read.
    if (height == 0) {
        Nan::ThrowError("Unable to read width from PNG.");
    }
    info.GetReturnValue().Set(Nan::New(height));
}

/**
 * This getter will return the bit depth of the image, gathered from `png_get_bit_depth`.
 */
NAN_GETTER(PngImage::getBitDepth) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    double bitDepth = png_get_bit_depth(pngImageInstance->pngPtr, pngImageInstance->infoPtr);
    info.GetReturnValue().Set(Nan::New(bitDepth));
}

/**
 * Used to convert the color type from libpng's internal enum format into strings.
 */
static string convertColorType(const png_byte &colorType) {
    switch (colorType) {
        case PNG_COLOR_TYPE_PALETTE: return "palette";
        case PNG_COLOR_TYPE_GRAY: return "gray";
        case PNG_COLOR_TYPE_GRAY_ALPHA: return "gray-alpha";
        case PNG_COLOR_TYPE_RGB: return "rgb";
        case PNG_COLOR_TYPE_RGB_ALPHA: return "rgb-alpha";
        default: return "unknown";
    }
}

/**
 * This getter will return the amount of channels of the image, gathered from `png_get_channels`.
 */
NAN_GETTER(PngImage::getChannels) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    double channels = png_get_channels(pngImageInstance->pngPtr, pngImageInstance->infoPtr);
    info.GetReturnValue().Set(Nan::New(channels));
}

/**
 * This getter will return the color type of the image as a string, gathered from `png_get_color_type`.
 */
NAN_GETTER(PngImage::getColorType) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    string colorType = convertColorType(png_get_color_type(pngImageInstance->pngPtr, pngImageInstance->infoPtr));
    info.GetReturnValue().Set(Nan::New(colorType).ToLocalChecked());
}

/**
 * Used to convert the interlace type from libpng's internal enum format into strings.
 */
static string convertInterlaceType(const png_byte &interlaceType) {
    switch (interlaceType) {
        case PNG_INTERLACE_NONE: return "none";
        case PNG_INTERLACE_ADAM7: return "adam7";
        default: return "unknown";
    }
}

/**
 * This getter will return the interlace type of the image as a string, gathered from `png_get_interlace_type`.
 */
NAN_GETTER(PngImage::getInterlaceType) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    string interlaceType = convertInterlaceType(png_get_interlace_type(pngImageInstance->pngPtr, pngImageInstance->infoPtr));
    info.GetReturnValue().Set(Nan::New<String>(interlaceType).ToLocalChecked());
}

/**
 * This getter will return the amount of bytes per row of the image, gathered from `png_get_rowbytes`.
 */
NAN_GETTER(PngImage::getRowBytes) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    double rowBytes = png_get_rowbytes(pngImageInstance->pngPtr, pngImageInstance->infoPtr);
    info.GetReturnValue().Set(Nan::New(rowBytes));
}

/**
 * This getter will return the horizontal offset of the image, gathered from `png_get_x_offset_pixels`.
 */
NAN_GETTER(PngImage::getOffsetX) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    double offsetX = png_get_x_offset_pixels(pngImageInstance->pngPtr, pngImageInstance->infoPtr);
    info.GetReturnValue().Set(Nan::New(offsetX));
}

/**
 * This getter will return the vertical offset of the image, gathered from `png_get_y_offset_pixels`.
 */
NAN_GETTER(PngImage::getOffsetY) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    double offsetY = png_get_y_offset_pixels(pngImageInstance->pngPtr, pngImageInstance->infoPtr);
    info.GetReturnValue().Set(Nan::New(offsetY));
}

/**
 * This getter will return the horizontal amount of pixels per meter of the image, gathered from `png_get_x_pixels_per_meter`.
 */
NAN_GETTER(PngImage::getPixelsPerMeterX) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    double pixelsPerMeterX = png_get_x_pixels_per_meter(pngImageInstance->pngPtr, pngImageInstance->infoPtr);
    info.GetReturnValue().Set(Nan::New(pixelsPerMeterX));
}

/**
 * This getter will return the vertical amount of pixels per meter of the image, gathered from `png_get_y_pixels_per_meter`.
 */
NAN_GETTER(PngImage::getPixelsPerMeterY) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    double pixelsPerMeterY = png_get_x_pixels_per_meter(pngImageInstance->pngPtr, pngImageInstance->infoPtr);
    info.GetReturnValue().Set(Nan::New(pixelsPerMeterY));
}

/**
 * Returns the buffer of data for the decodded image.
 */
NAN_GETTER(PngImage::getBuffer) {
    auto pngImageInstance = Nan::ObjectWrap::Unwrap<PngImage>(info.Holder());
    Local<Object> buffer = Nan::NewBuffer(reinterpret_cast<char*>(pngImageInstance->decoded), pngImageInstance->decodedSize).ToLocalChecked();
    info.GetReturnValue().Set(buffer);
}
