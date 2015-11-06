
#include <dnp3hammer/dnp3.h>

#include <stdio.h>

int TestParser(const HParser* parser, const uint8_t* data, size_t len)
{
    HParseResult *res = h_parse(parser, data, len);
    if (!res) {
        return -1;
    }

    h_parse_result_free(res);

    return 0;
}

int main (int argc, char *argv[])
{
    const size_t MAX_BYTES = 4096;

    // TODO - can the malloc's here every be reclaimed?
    dnp3_p_init();

    // read input from std-in until EOF
    uint8_t buffer[MAX_BYTES];

    //try to read from stdin
    const size_t NUM_READ = fread(buffer, 1, MAX_BYTES, stdin);

    printf("Processing %zu bytes", NUM_READ);

    /// run every test value through both parsers
    TestParser(dnp3_p_app_request, buffer, NUM_READ);
    printf("Invoking request parser");

    TestParser(dnp3_p_app_response, buffer, NUM_READ);
    printf("Invoking response parser");

    return 0;
}

