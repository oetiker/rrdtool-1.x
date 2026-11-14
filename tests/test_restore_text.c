/* A reader at end-of-document must fail instead of returning a value. */
#include "../src/rrd_restore.c"

/* One line per assertion so failure-only code adds no uncovered lines. */
#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "%s\n", msg); failed = 1; } } while (0)

int main(void)
{
    const char xml[] = "<rrd/>";
    xmlTextReaderPtr reader = xmlReaderForMemory(xml, sizeof(xml) - 1,
                                                NULL, NULL, XML_PARSE_NONET);
    xmlChar *text;
    int failed = 0;
    CHECK(reader != NULL, "cannot create XML reader");
    while (xmlTextReaderRead(reader) == 1)
        ;
    rrd_clear_error();
    text = get_xml_text(reader);
    xmlFreeTextReader(reader);
    CHECK(text == NULL, "get_xml_text returned a value at end of document");
    CHECK(rrd_test_error(), "get_xml_text set no error");
    CHECK(rrd_test_error() && strstr(rrd_get_error(), "file ended while looking for text"), "wrong error message");
    xmlFree(text);
    return failed;
}
