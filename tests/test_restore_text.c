/* A reader at end-of-document must fail instead of returning a value. */
#include "../src/rrd_restore.c"

int main(void)
{
    const char xml[] = "<rrd/>";
    xmlTextReaderPtr reader = xmlReaderForMemory(xml, sizeof(xml) - 1,
                                                NULL, NULL, XML_PARSE_NONET);
    xmlChar *text;
    if (reader == NULL)
        return 1;
    while (xmlTextReaderRead(reader) == 1)
        ;
    rrd_clear_error();
    text = get_xml_text(reader);
    xmlFreeTextReader(reader);
    if (text != NULL || !rrd_test_error() ||
        !strstr(rrd_get_error(), "file ended while looking for text")) {
        xmlFree(text);
        return 1;
    }
    return 0;
}
