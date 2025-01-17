#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

// ANSI color codes
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"      // Red for errors
#define COLOR_GREEN "\033[32m"    // Green for success messages
#define COLOR_ORANGE "\033[33m"   // Orange for warnings

// Function to update the Port node and return whether it was found
static int updatePortNode(xmlXPathObjectPtr xpathObj, const char* port_name, const char* capture_value) {
    // Track if a port node was found and updated
    int port_found = 0;

    // Iterate over the resulting nodes
    xmlNodeSetPtr nodes = xpathObj->nodesetval;
    for (int i = 0; i < nodes->nodeNr; i++) {
        xmlNodePtr portNode = nodes->nodeTab[i];
        xmlChar* name_attr = xmlGetProp(portNode, (const xmlChar *)"name");

        port_found = (name_attr && xmlStrcmp(name_attr, (const xmlChar *)port_name) == 0);
        xmlFree(name_attr);
        
        if (port_found) {
            // Update XML with new Connection node if port_name matches
            xmlNodePtr new_node = xmlNewChild(portNode, NULL, (const xmlChar *)"Connection", NULL);
            if (new_node == NULL) {
                fprintf(stderr, COLOR_RED "Error: unable to create new child node\n" COLOR_RESET);
                return -1;
            }
            xmlAttrPtr new_attr = xmlNewProp(new_node, (const xmlChar *)"other", (const xmlChar *)capture_value);
            if (new_attr == NULL) {
                fprintf(stderr, COLOR_RED "Error: unable to create new child node\n" COLOR_RESET);
                return -1;            
            }

            fprintf(stderr, COLOR_GREEN "%s : %s\n" COLOR_RESET, capture_value, port_name);
            break;
        }
    }

    return port_found; // Return whether a port node was found
}

static int generateCSV(xmlXPathObjectPtr xpathObj) {
    // Track if a port node was found and updated
    int port_found = 0;

    // Iterate over the resulting nodes
    xmlNodeSetPtr nodes = xpathObj->nodesetval;
    for (int i = 0; i < nodes->nodeNr; i++) {
        xmlNodePtr portNode = nodes->nodeTab[i];
        xmlChar* name_attr = xmlGetProp(portNode, (const xmlChar *)"name");
        
        if (name_attr) {
            // Find <Connection> child with "other" attribute containing "system:capture_X"
            xmlNodePtr connectionNode = portNode->children;
            while (connectionNode) {
                if (xmlStrcmp(connectionNode->name, (const xmlChar *)"Connection") == 0) {
                    int capture;
                    xmlChar* other_attr = xmlGetProp(connectionNode, (const xmlChar *)"other");
                    if (other_attr && (sscanf((const char *)other_attr, "system:capture_%d", &capture) == 1)) {
                        // Output to stdout in CSV format
                        printf("%d,%s\n", capture, (const char *)name_attr);
                        port_found++;

                        xmlFree(other_attr);
                        break;
                    }
                    xmlFree(other_attr);
                }
                connectionNode = connectionNode->next;
            }
        }
        xmlFree(name_attr);
    }

    return port_found; // Return whether a port node was found
}

static void processCSVAndUpdateXML(xmlXPathObjectPtr xpathObj) {
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        int capture;
        char port_name[50];
        
        // Read capture value and full port_name from the CSV input (stdin)
        if (sscanf(line, "%d,%49[^\n]", &capture, port_name) == 2) {
	        int port_found;
            char capture_value[50];
            snprintf(capture_value, sizeof(capture_value), "system:capture_%d", capture);

            // Update the XML document in memory and check if the port was found
            if ((port_found = updatePortNode(xpathObj, port_name, capture_value)) < 0) {
            	return;
            }
            
            // Print warning if the port was not found
            if (!port_found) {
                fprintf(stderr, COLOR_ORANGE "Warning: Port node '%s' not found for capture value %s\n" COLOR_RESET, port_name, capture_value);
            }
        } else {
            fprintf(stderr, COLOR_ORANGE "Warning: Invalid CSV format in line: %s" COLOR_RESET, line);
        }
    }
}

static void print_usage(const char* prog_name) {
    printf("Usage: %s -g ARDOUR_SESSION > MAPPING\n", prog_name);
    printf("  or:  %s ARDOUR_SESSION < MAPPING\n\n", prog_name);
    printf("Connect track's audio ports to input channels in an Ardour session XML file\n");
    printf("  -g, --generate             Output input channel:audio port mapping (CSV format)\n");
    printf("  -h, --help                 Guess what?\n");  
}

int main(int argc, char *argv[]) {
    int generate_csv = 0;
    char* xml_filename = NULL;

    struct option long_options[] = {
        {"generate", no_argument, 0, 'g'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "ghv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'g':
                generate_csv = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;           
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, COLOR_RED "Error: XML file must be specified\n" COLOR_RESET);
        print_usage(argv[0]);
        return 1;
    }
    xml_filename = argv[optind];

    // Load XML document
    xmlDocPtr doc = xmlReadFile(xml_filename, NULL, 0);
    if (doc == NULL) {
        fprintf(stderr, COLOR_RED "Error: Could not parse file %s\n" COLOR_RESET, xml_filename);
        return 1;
    }

    // Create XPath evaluation context
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (xpathCtx == NULL) {
        fprintf(stderr, COLOR_RED "Error: unable to create new XPath context\n" COLOR_RESET);
        xmlFreeDoc(doc);
        return 1;
    }

    // Evaluate XPath expression
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar *)"/Session/Routes/Route/IO[@direction='Input']/Port[@type='audio']", xpathCtx);
    if (xpathObj == NULL) {
        fprintf(stderr, COLOR_RED "Error: unable to evaluate xpath expression\n" COLOR_RESET);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return 1;
    }
    
    if (generate_csv) {
        // Generate CSV to stdout
        generateCSV(xpathObj);
    } else {
        // Update XML from CSV read from stdin
        processCSVAndUpdateXML(xpathObj);

        // Save the modified XML document after all updates
        if (xmlSaveFormatFileEnc(xml_filename, doc, "UTF-8", 1) == -1) {
            fprintf(stderr, COLOR_RED "Error: saving XML file %s\n" COLOR_RESET, xml_filename);
            xmlXPathFreeObject(xpathObj);
            xmlXPathFreeContext(xpathCtx);
            xmlFreeDoc(doc);
            return 1;
        }
    }

    // Cleanup
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return 0;
}

