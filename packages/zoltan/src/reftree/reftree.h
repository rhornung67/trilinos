/*****************************************************************************
 * CVS File Information :
 *    $RCSfile$
 *    $Author$
 *    $Date$
 *    $Revision$
 ****************************************************************************/

#ifndef __REFTREE_H
#define __REFTREE_H

#include "reftree_const.h"
/* Some constants */

/* Maximum number of vertices per element */
/* Used for dimensioning space for a query function to return vertices */
#define MAXVERT 8

/* Default dimension of the hash table */
#define DEFAULT_HASH_TABLE_SIZE 16384

/* Data structures for refinement tree */

/* The main refinement tree structure */

struct Zoltan_Reftree_Struct {
   ZOLTAN_ID_PTR global_id;  /* global ID of the corresponding element */
   ZOLTAN_ID_PTR local_id;   /* local ID of the corresponding element */
   struct Zoltan_Reftree_Struct *children; /* array of the children in the tree */
   int num_child;        /* number of children */
   float *weight;        /* weight of the node; dimension Obj_Weight_Dim */
   float *summed_weight; /* sum of the weights in the subtree rooted at
                            this node */
   float *my_sum_weight; /* sum of weights of nodes assigned to this proc */
   int num_vertex;       /* the number of vertices in the corresponding
                            element */
   int *vertices;        /* the vertices of the corresponding element;
                            local to this processor */
   int in_vertex;        /* starting vertex for determining the path through
                            the children */
   int out_vertex;       /* ending vertex for determining the path through
                            the children */
   int assigned_to_me;   /* for a leaf, 1 if this element is assigned to
                            this processor, 0 if not.  for nonleaves, 1 if
                            the entire subtree is assigned to this proc,
                            0 if none of the subtree, -1 if part */
   int partition;        /* partition to which this node is assigned;
                            meaningful only during the partition algorithm */
};

typedef struct Zoltan_Reftree_Struct ZOLTAN_REFTREE;

/* Hash table structures */

struct Zoltan_Reftree_hash_node {
  ZOLTAN_ID_PTR gid;            /* Global id */
  ZOLTAN_REFTREE *reftree_node; /* pointer to a node of the refinement tree */
  struct Zoltan_Reftree_hash_node *next;
};

/* data structure pointed to by zz->Data_Structure */

struct Zoltan_Reftree_data_struct {
  ZOLTAN_REFTREE *reftree_root;
  struct Zoltan_Reftree_hash_node **hash_table;
  int hash_table_size;
};

/* Prototypes */

extern int Zoltan_Reftree_Init(ZZ *zz);
extern int Zoltan_Reftree_Build(ZZ *zz);
extern void Zoltan_Reftree_Print(ZZ *zz,ZOLTAN_REFTREE *subroot, int level);

extern ZOLTAN_REFTREE* Zoltan_Reftree_hash_lookup(ZZ *zz, 
                                          struct Zoltan_Reftree_hash_node **hashtab,
                                          ZOLTAN_ID_PTR key, int n);
extern void Zoltan_Reftree_Hash_Insert(ZZ *zz, ZOLTAN_REFTREE *reftree_node,
                            struct Zoltan_Reftree_hash_node **hashtab, int size);
extern void Zoltan_Reftree_Hash_Remove(ZZ *zz, ZOLTAN_REFTREE *reftree_node,
                            struct Zoltan_Reftree_hash_node **hashtab, int size);
extern void Zoltan_Reftree_Clear_Hash_Table(struct Zoltan_Reftree_hash_node **hashtab,
                                 int size);


#endif /* __REFTREE_CONST_H */
