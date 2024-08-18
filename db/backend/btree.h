#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../../ds/hashtbl.h"
#include "../../util/alloc_util.h"

#define MAXKEYS 4
#define MAXCHILDS 5
#define MAX_KEYWORD_LENGTH 50
#define MAX_DOCS_PER_KEYWORD 1000
#define RECURSION_STACK_SIZE 2000
#ifndef BTREE_H
#define BTREE_H


typedef struct {
    char *keyword;
    char *value;
} keyword_entry;

typedef struct b_tree_node {
    int num_entries;
    keyword_entry *entries[MAXKEYS];
    struct b_tree_node *children[MAXCHILDS];
    bool isLeaf;
    bool traversed;
} b_tree_node;

typedef struct {
    b_tree_node *root;
    int numDocs;
} b_tree;

b_tree* create_b_tree(void);
b_tree_node* create_b_tree_node(void);
int insert_node(b_tree *tree, keyword_entry *entry);
keyword_entry* search(const char *target, b_tree *tree);
void split_node(b_tree_node *parent, int child_index);
void destroy_b_tree(b_tree *tree);
void insert_non_full(b_tree_node *root, keyword_entry *entry);
keyword_entry *create_entry(char * keyword);

b_tree_node* create_b_tree_node(void) {
    b_tree_node *node = (b_tree_node*)wrapper_alloc((sizeof(b_tree_node)), NULL,NULL);
    if (node == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    node->num_entries = 0;
    node->isLeaf = true;
    for (int i = 0; i < MAXCHILDS; i++) {
        node->children[i] = NULL;
    }
    for (int i = 0; i < MAXKEYS; i++) {
        node->entries[i] = NULL;
    }
    node->traversed = false;
    return node;
}

b_tree* create_b_tree(void) {
    b_tree *tree = (b_tree*)wrapper_alloc((sizeof(b_tree)), NULL,NULL);
    if (tree == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    tree->root = create_b_tree_node();
    tree->numDocs = 0;
    return tree;
}

void insertIntoNode(b_tree_node *node, keyword_entry *newEntry, int position) {
    for (int i = node->num_entries; i > position; i--) {
        node->entries[i] = node->entries[i - 1];
        if (!node->isLeaf) {
            node->children[i+1] = node->children[i];
        }
    }
    node->entries[position] = newEntry;
    node->num_entries++;
}
int insert_node(b_tree *tree, keyword_entry *entry) {
    if (tree == NULL || entry == NULL) {
        return -1;
    }
    
    b_tree_node *root = tree->root;
    if (root->num_entries == MAXKEYS) {
        b_tree_node *newRoot = create_b_tree_node();
        newRoot->isLeaf = false;
        newRoot->children[0] = root;
        tree->root = newRoot;
        split_node(newRoot, 0);
        insert_non_full(newRoot, entry);
    } else {
        insert_non_full(root, entry);
    }
    
    return 0;
}

void insert_non_full(b_tree_node *root, keyword_entry *entry) {
    b_tree_node *stack[RECURSION_STACK_SIZE];
    int stackTop = 0;
    int indexStack[RECURSION_STACK_SIZE];
    
    b_tree_node *node = root;
    
    while (true) {
        int i = node->num_entries - 1;
        
        if (node->isLeaf) {
            while (i >= 0 && strcmp(entry->keyword, node->entries[i]->keyword) < 0) {
                node->entries[i+1] = node->entries[i];
                i--;
            }
            node->entries[i+1] = entry;
            node->num_entries++;
            break;
        }
        else {
            while (i >= 0 && strcmp(entry->keyword, node->entries[i]->keyword) < 0) {
                i--;
            }
            i++;
            
            if (node->children[i]->num_entries == MAXKEYS) {
                split_node(node, i);
                if (strcmp(entry->keyword, node->entries[i]->keyword) > 0) {
                    i++;
                }
            }
            
            stack[stackTop] = node;
            indexStack[stackTop] = i;
            stackTop++;
            
            node = node->children[i];
        }
    }
    
    while (stackTop > 0) {
        stackTop--;
        b_tree_node *parent = stack[stackTop];
        int childIndex = indexStack[stackTop];
        
        if (parent->children[childIndex]->num_entries < MAXKEYS) {
            break;
        }
        
        split_node(parent, childIndex);
    }
}
void split_node(b_tree_node *parent, int childIndex) {
    b_tree_node *child = parent->children[childIndex];
    b_tree_node *newNode = create_b_tree_node();
    newNode->isLeaf = child->isLeaf;
    
    int midIndex = MAXKEYS / 2;
    keyword_entry *midEntry = child->entries[midIndex];
    
    if (child->isLeaf) {
        for (int j = 0; j < MAXKEYS - midIndex; j++) {
            newNode->entries[j] = child->entries[j + midIndex];
        }
        newNode->num_entries = MAXKEYS - midIndex;
        child->num_entries = midIndex;
    } 
    else {
        for (int j = 0; j < MAXKEYS - midIndex - 1; j++) {
            newNode->entries[j] = child->entries[j + midIndex + 1];
            newNode->children[j] = child->children[j + midIndex + 1];
        }
        newNode->children[MAXKEYS - midIndex - 1] = child->children[MAXKEYS];
        newNode->num_entries = MAXKEYS - midIndex - 1;
        child->num_entries = midIndex;
    }
    
    for (int j = parent->num_entries; j > childIndex; j--) {
        parent->children[j + 1] = parent->children[j];
        parent->entries[j] = parent->entries[j - 1];
    }
    
    parent->entries[childIndex] = midEntry;
    parent->children[childIndex+1] = newNode;
    parent->num_entries++;
    //parent->isLeaf = false;
}
keyword_entry* search(const char *target, b_tree *tree) {
    if (target == NULL || tree == NULL) return NULL;
    b_tree_node *current = tree->root;
    
    while (current != NULL) {
        int i = 0;
        while (i < current->num_entries && strcmp(target, current->entries[i]->keyword) > 0) {
            i++;
        }
        
        if (i < current->num_entries && strcmp(target, current->entries[i]->keyword) == 0) {
            return current->entries[i];
        }
        
        if (current->isLeaf) break;
        
        current = current->children[i];
    }
    
    return NULL;
}
void destroyBTreeNode(b_tree_node *node) {
    if (node == NULL) return;
    
    if (!node->isLeaf) {
        for (int i = 0; i <= node->num_entries; i++) {
            destroyBTreeNode(node->children[i]);
        }
    }
    
    free(node);
    node = NULL;
}

void destroy_b_tree(b_tree *tree) {
    if (tree == NULL) return;
    destroyBTreeNode(tree->root);
    free(tree);
}
keyword_entry* create_entry(char * entry){
    if (entry == NULL) return NULL;
    keyword_entry * key = wrapper_alloc((sizeof(*key)), NULL,NULL);
    key->keyword= entry;
    return key;
}
void printBTreeNode(b_tree_node *node, int level) {
    if (node == NULL) return;
    for (int i = 0; i < node->num_entries; i++) {
        if (!node->isLeaf) {
            printBTreeNode(node->children[i], level + 1);
        }
        for (int j = 0; j < level; j++) printf("    ");
        printf("%s (%d)\n", node->entries[i]->keyword, level);
    }
    if (!node->isLeaf) {
        printBTreeNode(node->children[node->num_entries], level + 1);
    }
}

void pretty_print_b_tree(b_tree *tree) {
    if (tree == NULL || tree->root == NULL) {
        printf("Empty tree\n");
        return;
    }
    printBTreeNode(tree->root, 0);
}
#endif