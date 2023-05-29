#include <stdio.h>
#include <stdlib.h>
#include <stdint-gcc.h>
#include "err.h"

typedef struct tree_node tree_node;

struct tree_node {
    tree_node *left;
    uint64_t num;
    tree_node *right;
};

tree_node *init_node(uint64_t num) {
    tree_node *node = malloc(sizeof(tree_node));
    node->num = num;
    node->left = node->right = NULL;
    return node;
}

tree_node *insert(tree_node *root, uint64_t num) {
    if (!root)
        return init_node(num);
    else if (root->num < num)
        root->left = insert(root->left, num);
    else if (root->num > num)
        root->right = insert(root->right, num);

    return root;
}

uint64_t tree_to_arr(tree_node *root, uint64_t **arr, uint64_t *arr_size) {
    uint64_t count = 0;
    if (root) {
        count += tree_to_arr(root->left, arr, arr_size);
        if (*arr_size == count) {
            if (*arr_size == 0) *arr_size = 1;
            else *arr_size *= 2;
            *arr = realloc(*arr, *arr_size * sizeof(uint64_t));
            if (!(*arr))
                fatal("realloc");
        }
        (*arr)[count++] = root->num;
        count += tree_to_arr(root->right, arr, arr_size);
    }
    return count;
}


int main() {
    tree_node *t = NULL;
    for (int i = 0; i < 10; i++)
        t = insert(t, i);
    for (int i = 0; i < 10; i++)
        t = insert(t, i);

    uint64_t *arr = NULL;
    uint64_t arr_size = 0;
    uint64_t count = tree_to_arr(t, &arr, &arr_size);

    for (int i = 0; i < count; i++)
        printf("%lu\n", arr[i]);

}