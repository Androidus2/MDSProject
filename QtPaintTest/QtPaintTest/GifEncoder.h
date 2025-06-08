#include <QtWidgets>
// Update GifEncoder class in the header

class GifEncoder {
public:
    GifEncoder(const QString& fileName, int width, int height, int delay, bool loop);
    ~GifEncoder();

    bool addFrame(const QImage& image);
    bool finish();

private:
    struct LZWNode {
        int code;
        int parent;
        int firstChild;
        int nextSibling;
        int character;
    };

    bool writeHeader();
    bool writeLogicalScreenDescriptor();
    bool writeNetscapeExtension();
    bool writeGraphicControlExtension(int delay);
    bool writeImageDescriptor(int left, int top, int width, int height);
    bool writeColorTable(const QVector<QRgb>& colorTable);
    bool writeLZWCompressedData(const QByteArray& indexedData);

    QFile m_file;
    int m_width;
    int m_height;
    int m_delay;
    bool m_loop;
    bool m_headerWritten;
    int m_frameCount;

    // LZW compression helpers
    void initLZWDictionary(int colorDepth);
    void compressLZW(const QByteArray& indexedData, QByteArray& compressed);
    void writeBits(QByteArray& output, int value, int bitCount, int& bitPosition);
    void packBits(QByteArray& output, int& bitPosition);

    // Color quantization helper
    QVector<QRgb> createColorTable(const QImage& image, int maxColors = 256);
    QByteArray quantizeImage(const QImage& image, const QVector<QRgb>& colorTable);

    // LZW dictionary
    QVector<LZWNode> m_lzwNodes;
    int m_nextCode;
    int m_colorDepth;
};