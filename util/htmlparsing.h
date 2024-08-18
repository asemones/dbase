#include <stdio.h>
#include <string.h>
#include <errno.h>
#include </opt/homebrew/Cellar/tidy-html5/5.8.0/include/tidy.h>
#include </opt/homebrew/Cellar/tidy-html5/5.8.0/include/tidybuffio.h>
#include "../ds/hashtbl.h"
#include "alloc_util.h"


#ifndef HTMLPARSEING_H
#define HTMLPARSEING_H

 /*This is really a wrapper for tidyHTML. Implement more functionality as needed*/
/**
 * Cleans the body of an HTML document.
 *
 * @param tdoc A pointer to the Tidy document structure.
 * @param body A pointer to the character array containing the body to be cleaned.
 * @return A TidyBuffer struct containing the cleaned body.
 */
TidyBuffer clean_body(TidyDoc *tdoc, char *body);

/**
 * Finds all text within a Tidy document node and appends it to a buffer.
 *
 * @param tdoc The Tidy document structure.
 * @param tnod The Tidy node to search within.
 * @param buf A pointer to the TidyBuffer struct to append the text to.
 * @param txt A pointer to the character array to store the found text.
 */
void find_all_text(TidyDoc tdoc, TidyNode tnod, TidyBuffer *buf, char *txt);

/**
 * Retrieves the string representation of a Tidy node type.
 *
 * @param nt The Tidy node type.
 * @return A character pointer to the string representation of the node type.
 */
char *get_node_type(TidyNodeType nt);

/**
 * Finds a specific tag within a Tidy document and appends it to a buffer.
 *
 * @param tdoc The Tidy document structure.
 * @param root_node The root node of the document.
 * @param buf A pointer to the TidyBuffer struct to append the tag to.
 * @param tag A pointer to the character array representing the tag to find.
 * @param txt A pointer to the character array to store the found tag.
 * @param classFilter Classes to include 
 * @param styleFilter css to include
 */
void find_tag(TidyDoc tdoc, TidyNode root_node, TidyBuffer *buf, char *tag, char *txt, list * attr, list * temp);

/**
 * Extracts URLs from an anchor tag and appends them to a list.
 *
 * @param anchor A pointer to the character array containing the anchor tag.
 * @param buf_list A pointer to the list structure to append the URLs to.
 * @return An integer indicating the number of URLs extracted.
 */
int yoink_url(char *anchor, list *bufList, char * domain);

/**
 * Cleans text by removing unwanted characters and formatting.
 *
 * @param text A pointer to the character array containing the text to be cleaned.
 * @return An integer indicating the success (1) or failure (0) of the cleaning operation.
 */
int clean_text(char *text);


TidyBuffer clean_body(TidyDoc *tdoc, char * body)
{
  const char* input = body;
  TidyBuffer output = {0};
  if (tdoc == NULL || body == NULL){
    return output;
  }
  TidyBuffer errbuf = {0};
  int status = -1;
  Bool ok;
                    

 
  ok = tidyOptSetBool( *tdoc, TidyXhtmlOut, yes );  
  if (ok)
    status = tidySetErrorBuffer(*tdoc, &errbuf);      
  if (status >= 0)
    status = tidyParseString(*tdoc, input);           
  if (status >= 0)
    status = tidyCleanAndRepair(*tdoc);               
  if (status >= 0)
    status = tidyRunDiagnostics(*tdoc);               
  if (status > 1)                                    
    status = ( tidyOptSetBool(*tdoc, TidyForceOutput, yes) ? status : -1 );
  if (status >= 0)
    status = tidySaveBuffer( *tdoc, &output );      

  if (status < 0){
    printf( "A severe error (%d) occurred.\n", status );
  }
  tidyBufFree( &errbuf );
  return output;
}
char *get_node_type( TidyNodeType nt )
{
    switch (nt)
    {
    case TidyNode_Root: return "Root";
    case TidyNode_DocType: return "DOCT";   // "DOCTYPE";
    case TidyNode_Comment: return "Comm";   // "Comment";
    case TidyNode_ProcIns: return "PrIn"; /**< Processing Instruction */
    case TidyNode_Text: return "Text";  /**< Text */
    case TidyNode_Start: return "Star"; /**< Start Tag */
    case TidyNode_End: return  "Endt";    /**< End Tag */
    case TidyNode_StartEnd: return "Clos";    /**< Start/End (empty) Tag */
    case TidyNode_CDATA: return "CDAT";     /**< Unparsed Text */
    case TidyNode_Section: return "XMLs";     /**< XML Section */
    case TidyNode_Asp: return "ASPs";         /**< ASP Source */
    case TidyNode_Jste: return "JSTE";       /**< JSTE Source */
    case TidyNode_Php: return "PHPs";         /**< PHP Source */
    case TidyNode_XmlDecl: return "XMLd";
    }
    return "UNKN";
}
void find_all_text(TidyDoc tdoc, TidyNode tnod, TidyBuffer* buf, char* txt) {
    TidyNode child;
    for (child = tidyGetChild(tnod); child; child = tidyGetNext(child)) {
        if (strcmp(get_node_type(tidyNodeGetType(child)), "Text") == 0) {
            tidyNodeGetText(tdoc, child, buf);
            strcat(txt, (char*)buf->bp); // Append the text to the txt buffer
            tidyBufClear(buf); // Clear the buffer for the next text node
        }
        find_all_text(tdoc, child, buf, txt); // Recursive call for child nodes
    }
    return;
}

void get_all_attributes(TidyNode root, list *kv_list){
    if (root== NULL){
      return;
    }
    TidyAttr temp = tidyAttrFirst(root);
    if (temp== NULL){
      return;
    }
    kv * temp2 =  dynamic_kv((void*)tidyAttrName(temp),  (void*)tidyAttrValue(temp));

    insert(kv_list, temp2);
 
    while((temp = tidyAttrNext(temp))!=NULL){
        kv * temp3 =  dynamic_kv((void*)tidyAttrName(temp),  (void*)tidyAttrValue(temp));
        insert(kv_list, temp3);
       
    }
    sort_list(kv_list,true,compare_values_by_key);
}
bool screen_attributes(list * screen_v, list * actual_v){
    if (screen_v== NULL || actual_v == NULL){
        return true;
    }
    int j = 0;
    for (int i = 0; i < screen_v->len; i++){
       kv * temp_attr = (kv*)get_element(screen_v,i);
      for ( j = 0; j < actual_v->len; j++){
        kv * temp_list = (kv*)get_element(actual_v,j);
        if (temp_list== NULL){
            return false;
        }
        if (strcmp(temp_attr->key, temp_list->key) == 0){
            if (strcmp(temp_attr->value, temp_list->value) != 0){
                return false;
            }
        }
      }
    }
    if (j < actual_v->len) {
        return false;  
    }
    return true;  

}
void find_tag(TidyDoc tdoc, TidyNode root_node, TidyBuffer* buf,char * tag,char * txt, list * attr, list * temp){
    TidyNode stack[100000];
    int stack_index = 0;
    
    stack[stack_index++] = root_node;

    if (attr != NULL) {
        sort_list(attr, true, compare_values_by_key);
    }

    while (stack_index > 0) {
        TidyNode node = stack[--stack_index];
        
        const char * t = tidyNodeGetName(node);
        if (t != NULL && strcmp(tag, t) == 0) {
            if (attr != NULL && temp != NULL) {
                get_all_attributes(node, temp);
                if (!screen_attributes(attr, temp)) {
                    continue;
                }
            }
            tidyNodeGetText(tdoc, node, buf);
            strcat(txt, (char*)buf->bp);
            tidyBufClear(buf);
            
        }
        for (TidyNode child = tidyGetChild(node); child; child = tidyGetNext(child)) {
            stack[stack_index++] = child;
        }

        tidyBufClear(buf);
    }
}
int yoink_url(char *anchor, list *buf_list, char * domain) {
    char *ptr;
    if (buf_list == NULL || buf_list->dtS != 8 || anchor == NULL) {
        return -1;
    }
    char *temp = anchor;
    char *url = NULL;
    while ((ptr = strstr(temp, "href=")) != NULL) {
        ptr  = strstr(ptr, "\"")+1;
        if (ptr == NULL){
           continue;
        }
        char *temp2 = strstr(ptr, "\"");
        size_t len = temp2 - ptr;
        url = wrapper_alloc((len+5), NULL,NULL);
        if (url == NULL) {
            return -1; 
        }
        memcpy(url, ptr, len);
        url[len] = '\0';
        build_sub_directories(domain, &url);
        if (attempt_repair(&url, true) == 0){
           insert(buf_list, &url);
        }
        else{
          free(url);
        }
        temp = temp2;
    }

    return 0;
}
int clean_text(char* text) {
    if (text == NULL) {
        return -1;
    }    
    int l = strlen(text);
    int temp_index = 0;
    bool inside_block = false;
    
   for (int i = 0; i < l; i++) {
      if (text[i] == '<') {
          inside_block = true;
      } 
      else if (text[i] == '>') {
          inside_block = false;
      } 
      else if (!inside_block) {
          text[temp_index++] = text[i];
      }
    }
      text[temp_index] = '\0';
  return 0;
}





#endif