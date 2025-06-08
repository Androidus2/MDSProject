#include "GifEncoder.h"
// Updated GifEncoder implementation

GifEncoder::GifEncoder(const QString& fileName, int width, int height, int delay, bool loop)
    : m_width(width), m_height(height), m_delay(delay), m_loop(loop),
    m_headerWritten(false), m_frameCount(0)
{
    m_file.setFileName(fileName);
    m_file.open(QIODevice::WriteOnly);
}

GifEncoder::~GifEncoder()
{
    if (m_file.isOpen()) {
        finish();
    }
}

bool GifEncoder::addFrame(const QImage& image)
{
    if (!m_file.isOpen()) {
        return false;
    }

    // Scale image if needed
    QImage frameImage = image;
    if (frameImage.width() != m_width || frameImage.height() != m_height) {
        frameImage = frameImage.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // Write header for the first frame
    if (!m_headerWritten) {
        if (!writeHeader() || !writeLogicalScreenDescriptor()) {
            return false;
        }

        // Write Netscape extension for looping
        if (m_loop && !writeNetscapeExtension()) {
            return false;
        }

        m_headerWritten = true;
    }

    // Create color table (with max 256 colors for GIF)
    QVector<QRgb> colorTable = createColorTable(frameImage);

    // Quantize image to use this color table
    QByteArray indexedData = quantizeImage(frameImage, colorTable);

    // Write the frame
    if (!writeGraphicControlExtension(m_delay) ||
        !writeImageDescriptor(0, 0, m_width, m_height) ||
        !writeColorTable(colorTable) ||
        !writeLZWCompressedData(indexedData)) {
        return false;
    }

    m_frameCount++;
    return true;
}

bool GifEncoder::finish()
{
    if (!m_file.isOpen()) {
        return false;
    }

    // Write trailer
    m_file.write(QByteArray(1, 0x3B));  // GIF trailer
    m_file.close();
    return true;
}

bool GifEncoder::writeHeader()
{
    // GIF signature and version
    return m_file.write("GIF89a", 6) == 6;
}

bool GifEncoder::writeLogicalScreenDescriptor()
{
    QByteArray descriptor(7, 0);

    // Width and height (little endian)
    descriptor[0] = m_width & 0xFF;
    descriptor[1] = (m_width >> 8) & 0xFF;
    descriptor[2] = m_height & 0xFF;
    descriptor[3] = (m_height >> 8) & 0xFF;

    // Packed fields:
    // - Global Color Table Flag = 0 (we'll use local color tables)
    // - Color Resolution = 7 (0x70)
    // - Sort Flag = 0 (0x00)
    // - Size of Global Color Table = 0 (we'll use local color tables)
    descriptor[4] = 0x00 | 0x70 | 0x00;

    // Background color index
    descriptor[5] = 0;

    // Pixel aspect ratio
    descriptor[6] = 0;

    return m_file.write(descriptor) == descriptor.size();
}

bool GifEncoder::writeNetscapeExtension()
{
    // Extension introducer
    QByteArray netscape;
    netscape.append(char(0x21));  // Extension Introducer
    netscape.append(char(0xFF));  // Application Extension Label

    // Application identifier block
    netscape.append(char(11));    // Block size
    netscape.append("NETSCAPE2.0", 11); // Application identifier and code

    // Sub-block
    netscape.append(char(3));     // Sub-block size
    netscape.append(char(1));     // Sub-block ID (1 for loop count)

    // Loop count (0 = infinite)
    netscape.append(char(0));     // Loop count (low byte)
    netscape.append(char(0));     // Loop count (high byte)

    // Block terminator
    netscape.append(char(0));     // Terminator

    return m_file.write(netscape) == netscape.size();
}

bool GifEncoder::writeGraphicControlExtension(int delay)
{
    QByteArray gce;

    // Extension introducer
    gce.append(char(0x21));  // Extension Introducer
    gce.append(char(0xF9));  // Graphic Control Extension Label

    // Block size
    gce.append(char(4));     // Size of the block (always 4)

    // Packed field
    // - 3 bits: Reserved
    // - 3 bits: Disposal method (2 = restore to background)
    // - 1 bit: User input flag (0 = no user input)
    // - 1 bit: Transparent color flag (1 = transparent color used)
    gce.append(char(0x09));  // 00001001 - Disposal = 1 (keep), Transparent color flag = 1

    // Delay time (in 1/100 seconds)
    int centiseconds = delay / 10; // Convert from milliseconds to 1/100 seconds
    gce.append(char(centiseconds & 0xFF));
    gce.append(char((centiseconds >> 8) & 0xFF));

    // Transparent color index (0 = first color in palette)
    gce.append(char(0));

    // Block terminator
    gce.append(char(0));

    return m_file.write(gce) == gce.size();
}

bool GifEncoder::writeImageDescriptor(int left, int top, int width, int height)
{
    QByteArray descriptor(10, 0);

    // Image separator
    descriptor[0] = 0x2C;

    // Image left position (little endian)
    descriptor[1] = left & 0xFF;
    descriptor[2] = (left >> 8) & 0xFF;

    // Image top position (little endian)
    descriptor[3] = top & 0xFF;
    descriptor[4] = (top >> 8) & 0xFF;

    // Image width (little endian)
    descriptor[5] = width & 0xFF;
    descriptor[6] = (width >> 8) & 0xFF;

    // Image height (little endian)
    descriptor[7] = height & 0xFF;
    descriptor[8] = (height >> 8) & 0xFF;

    // Packed fields:
    // - Local Color Table Flag = 1 (0x80)
    // - Interlace Flag = 0 (0x00)
    // - Sort Flag = 0 (0x00)
    // - Reserved = 0 (0x00)
    // - Size of Local Color Table = 7 (0x07) (2^(7+1) = 256 colors)
    descriptor[9] = 0x80 | 0x07;

    return m_file.write(descriptor) == descriptor.size();
}

bool GifEncoder::writeColorTable(const QVector<QRgb>& colorTable)
{
    QByteArray rawColorTable;

    // Make sure we have at least 2 colors (required by GIF)
    int colorCount = qMax(2, colorTable.size());
    colorCount = qMin(256, colorCount); // Max 256 colors for 8-bit GIF

    // Pad the color table to a power of 2 (required by GIF)
    int paddedSize = 256; // Always use 256 colors for 8-bit GIF

    // Write RGB triplets for each color
    for (int i = 0; i < colorCount; i++) {
        QRgb color = colorTable[i];
        rawColorTable.append(char(qRed(color)));
        rawColorTable.append(char(qGreen(color)));
        rawColorTable.append(char(qBlue(color)));
    }

    // Pad with black if needed
    for (int i = colorCount; i < paddedSize; i++) {
        rawColorTable.append(char(0));
        rawColorTable.append(char(0));
        rawColorTable.append(char(0));
    }

    return m_file.write(rawColorTable) == rawColorTable.size();
}

// Improved LZW compression
void GifEncoder::initLZWDictionary(int colorDepth)
{
    m_colorDepth = colorDepth;
    int maxCode = (1 << colorDepth); // Number of colors

    // Initialize dictionary with basic codes (0-255 for colors + 2 special codes)
    m_lzwNodes.resize(4096); // Max possible codes for 12-bit LZW

    for (int i = 0; i < m_lzwNodes.size(); i++) {
        m_lzwNodes[i].code = i;
        m_lzwNodes[i].parent = -1;
        m_lzwNodes[i].firstChild = -1;
        m_lzwNodes[i].nextSibling = -1;
        m_lzwNodes[i].character = i;
    }

    // Start with codes right after the clear code and end code
    m_nextCode = maxCode + 2;
}

void GifEncoder::writeBits(QByteArray& output, int value, int bitCount, int& bitPosition)
{
    for (int i = 0; i < bitCount; i++) {
        if (value & (1 << i)) {
            // Set the bit
            int bytePos = bitPosition / 8;
            int bitPos = bitPosition % 8;

            // Ensure the output is large enough
            while (output.size() <= bytePos) {
                output.append(char(0));
            }

            output[bytePos] |= (1 << bitPos);
        }
        bitPosition++;
    }
}

void GifEncoder::packBits(QByteArray& output, int& bitPosition)
{
    // Make sure we have a complete byte
    if (bitPosition % 8 != 0) {
        bitPosition = ((bitPosition / 8) + 1) * 8;
    }
}

bool GifEncoder::writeLZWCompressedData(const QByteArray& indexedData)
{
    // For simplicity, we'll use a fixed code size approach
    // with clear codes periodically to reset the dictionary

    // LZW minimum code size (8 for 256 colors)
    int minCodeSize = 8;
    m_file.write(QByteArray(1, minCodeSize));

    // Prepare for LZW compression
    QByteArray compressed;

    // Perform simplified LZW compression
    int clearCode = 1 << minCodeSize;
    int endCode = clearCode + 1;
    int codeBits = minCodeSize + 1;

    // Initialize dictionary
    QHash<QByteArray, int> dictionary;
    for (int i = 0; i < clearCode; i++) {
        dictionary[QByteArray(1, char(i))] = i;
    }

    // Add clear and end codes
    dictionary[QByteArray(1, char(clearCode))] = clearCode;
    dictionary[QByteArray(1, char(endCode))] = endCode;

    // Add first clear code
    compressed.append(char(clearCode & 0xFF));
    compressed.append(char((clearCode >> 8) & 0xFF));

    // Compress data
    QByteArray currentSequence;
    for (char byte : indexedData) {
        QByteArray newSequence = currentSequence;
        newSequence.append(byte);

        if (dictionary.contains(newSequence)) {
            currentSequence = newSequence;
        }
        else {
            // Output the code for currentSequence
            int code = dictionary[currentSequence];
            compressed.append(char(code & 0xFF));
            if (code >= 256) {
                compressed.append(char((code >> 8) & 0xFF));
            }

            // Add new sequence to dictionary if not full
            if (dictionary.size() < 4096) {
                dictionary[newSequence] = dictionary.size();

                // If we've used all codes at current bit length, increment
                if (dictionary.size() >= (1 << codeBits) && codeBits < 12) {
                    codeBits++;
                }
            }
            else {
                // Dictionary full, output clear code and reset
                compressed.append(char(clearCode & 0xFF));
                compressed.append(char((clearCode >> 8) & 0xFF));

                // Reset dictionary
                dictionary.clear();
                for (int i = 0; i < clearCode; i++) {
                    dictionary[QByteArray(1, char(i))] = i;
                }
                dictionary[QByteArray(1, char(clearCode))] = clearCode;
                dictionary[QByteArray(1, char(endCode))] = endCode;

                codeBits = minCodeSize + 1;
            }

            // Start new sequence with current byte
            currentSequence.clear();
            currentSequence.append(byte);
        }
    }

    // Output code for the last sequence
    if (!currentSequence.isEmpty()) {
        int code = dictionary[currentSequence];
        compressed.append(char(code & 0xFF));
        if (code >= 256) {
            compressed.append(char((code >> 8) & 0xFF));
        }
    }

    // End with end code
    compressed.append(char(endCode & 0xFF));
    compressed.append(char((endCode >> 8) & 0xFF));

    // Write compressed data in 255-byte chunks
    for (int i = 0; i < compressed.size(); i += 255) {
        int chunkSize = qMin(255, compressed.size() - i);
        m_file.write(QByteArray(1, chunkSize));
        m_file.write(compressed.mid(i, chunkSize));
    }

    // End with empty block
    m_file.write(QByteArray(1, 0));

    return true;
}

QVector<QRgb> GifEncoder::createColorTable(const QImage& image, int maxColors)
{
    // Convert to indexed format with Qt's dithering
    QImage indexed = image.convertToFormat(QImage::Format_Indexed8,
        Qt::ColorOnly | Qt::ThresholdDither);

    // Get the color table
    QVector<QRgb> colorTable = indexed.colorTable();

    // Make sure transparent color is at index 0
    bool hasTransparent = false;
    for (int i = 0; i < colorTable.size(); i++) {
        if (qAlpha(colorTable[i]) < 128) {
            // Already has a transparent color
            if (i != 0) {
                // Move it to index 0
                QRgb transparent = colorTable[i];
                colorTable.removeAt(i);
                colorTable.prepend(transparent);
            }
            hasTransparent = true;
            break;
        }
    }

    // If no transparent color found, add one at index 0
    if (!hasTransparent) {
        colorTable.prepend(qRgba(0, 0, 0, 0));
    }

    // Limit to maxColors
    if (colorTable.size() > maxColors) {
        colorTable.resize(maxColors);
    }

    // Make sure we have at least 2 colors (GIF requirement)
    if (colorTable.size() < 2) {
        colorTable.append(qRgb(255, 255, 255)); // Add white
    }

    return colorTable;
}

QByteArray GifEncoder::quantizeImage(const QImage& image, const QVector<QRgb>& colorTable)
{
    // Create an indexed image with our color table
    QImage indexed(image.size(), QImage::Format_Indexed8);
    indexed.setColorTable(colorTable);

    // Fill with transparent color initially
    indexed.fill(0);

    // Convert each pixel to the nearest color in the table
    QByteArray indexedData(image.width() * image.height(), 0);

    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            QRgb pixelColor = image.pixel(x, y);

            // Handle transparency
            if (qAlpha(pixelColor) < 128) {
                indexedData[y * image.width() + x] = 0; // Transparent color index
                continue;
            }

            // Find the nearest color in the table
            int bestIndex = 1; // Start at 1 to skip transparent
            int bestDistance = INT_MAX;

            for (int i = 1; i < colorTable.size(); i++) {
                QRgb tableColor = colorTable[i];

                // Calculate color distance (simple RGB distance)
                int dr = qRed(pixelColor) - qRed(tableColor);
                int dg = qGreen(pixelColor) - qGreen(tableColor);
                int db = qBlue(pixelColor) - qBlue(tableColor);
                int distance = dr * dr + dg * dg + db * db;

                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestIndex = i;
                }
            }

            indexedData[y * image.width() + x] = bestIndex;
            indexed.setPixel(x, y, bestIndex);
        }
    }

    return indexedData;
}