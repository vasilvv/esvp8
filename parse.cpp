#include <fstream>
#include <stdint.h>

#define FRAME_SIZE_SANITY_CHECK 33554432

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t frame_rate;
    uint32_t time_scale;
    uint32_t num_frames;
} ivf_header;

typedef struct {
    uint32_t size;
    uint64_t timestamp;
} ivf_frame_header;

/* Based on RFC 6386 */
typedef struct {
    bool key_frame;
    uint8_t version;
    bool show_frame;
} vp8_frame_header;

ivf_header read_ivf_header(std::ifstream& input) {
    char buffer[32];
    ivf_header header;

    input.read( buffer, 32 );
    if( !input ) {
        throw "Failed to read the header";
    }

    if( buffer[0] != 'D' || buffer[1] != 'K' || buffer[2] != 'I' || buffer[3] != 'F' ) {
        throw "Invalid file";
    }

    // FIXME: check the VP8 FourCC

    header.width = *reinterpret_cast<uint16_t*>(&buffer[12]);
    header.height = *reinterpret_cast<uint16_t*>(&buffer[14]);
    header.frame_rate = *reinterpret_cast<uint32_t*>(&buffer[16]);
    header.time_scale = *reinterpret_cast<uint32_t*>(&buffer[20]);
    header.num_frames = *reinterpret_cast<uint32_t*>(&buffer[24]);

    return header;
}

ivf_frame_header read_ivf_frame_header(std::ifstream& input) {
    char buffer[12];
    ivf_frame_header header;

    input.read( buffer, 12 );

    header.size = *reinterpret_cast<uint32_t*>(&buffer[0]);
    header.timestamp = *reinterpret_cast<uint64_t*>(&buffer[4]);
    return header;
}

vp8_frame_header parse_vp8_frame_header(char *buffer, uint32_t buffer_len) {
    vp8_frame_header header;
    volatile uint32_t tag;

    if( buffer_len < 3 ) {
        throw "The frame is too short!";
    }

    /* Based on RFC 6386 */
    tag = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16);
    header.key_frame = (tag & 0x01) == 0;
    header.version = (tag >> 1) & 0x07;
    header.show_frame = (tag >> 5) & 0x7FFFF;
    return header;
}

int main(int argc, char* argv[]) {
    std::ifstream input( argv[1], std::ifstream::binary );
    ivf_header header;

    header = read_ivf_header(input);
    printf( "%ix%i, %.02f fps, %i frames\n",
        header.width, header.height,
        ((double)header.frame_rate / header.time_scale),
        header.num_frames );

    for( int i = 0; i < header.num_frames; i++ ) {
        ivf_frame_header fhdr;
        char* buffer;

        fhdr = read_ivf_frame_header(input);
        if( fhdr.size > FRAME_SIZE_SANITY_CHECK ) {
            throw "Suspiciously long frame discovered";
        }

        buffer = new char[fhdr.size];
        input.read( buffer, fhdr.size );
        if( !input ) {
            throw "Unable to read frame";
        }

        vp8_frame_header vheader;
        vheader = parse_vp8_frame_header(buffer ,fhdr.size);

        printf( "Frame %u, length %u\n", i, fhdr.size );
        printf( "   %s, version %i, %s\n",
            vheader.key_frame ? "I-frame" : "P-frame",
            vheader.version,
            vheader.show_frame ? "Displayed" : "Hidden" );

        delete buffer;
    }

    // FIXME: needs proper exception handling
}
