/*****************************************************************************
 * Zoltan Library for Parallel Applications                                  *
 * Copyright (c) 2000,2001,2002, Sandia National Laboratories.               *
 * For more info, see the README file in the top-level Zoltan directory.     *
 *****************************************************************************/
/*****************************************************************************
 * CVS File Information :
 *    $RCSfile$
 *    $Author$
 *    $Date$
 *    $Revision$
 ****************************************************************************/

#ifdef __cplusplus
/* if C++, define the rest of this header file as extern C */
extern "C" {
#endif

#include "phg.h"
#include "zz_const.h"
#include "parmetis_jostle.h"
#include "zz_util_const.h"
#include "hg_hypergraph.h"
    
#define MEMORY_ERROR { \
  ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Memory error."); \
  ierr = ZOLTAN_MEMERR; \
  goto End; \
}

/*****************************************************************************/
/* Function prototypes */

static int hash_lookup (ZZ*, ZOLTAN_ID_PTR, int, struct Hash_Node**);
static int Zoltan_PHG_Fill_Hypergraph (ZZ*, ZPHG*, Partition*);

/*****************************************************************************/

int Zoltan_PHG_Build_Hypergraph(
  ZZ *zz,                            /* Zoltan data structure */
  ZPHG **zoltan_hg,                  /* Hypergraph to be allocated and built.*/
  Partition *input_parts,            /* Initial partition assignments for
                                        vtxs in 2D distribution; length = 
                                        zoltan_hg->PHG->nVtx.  */
  PHGPartParams *hgp                 /* Parameters for HG partitioning.*/
)
{
/* allocates and builds hypergraph data structure using callback routines */ 
ZPHG *zhg;                     /* Temporary pointer to Zoltan_HGraph. */
HGraph *phgraph;             /* Temporary pointer to HG field */
int ierr = ZOLTAN_OK;
char *yo = "Zoltan_PHG_Build_Hypergraph";

  ZOLTAN_TRACE_ENTER(zz, yo);

  /* Allocate a Zoltan hypergraph.  */
  zhg = *zoltan_hg = (ZPHG*) ZOLTAN_MALLOC (sizeof(ZPHG));
  if (zhg == NULL) MEMORY_ERROR;

  /* Initialize the Zoltan hypergraph data fields. */
  zhg->GIDs = NULL;
  zhg->LIDs = NULL;
  zhg->Input_Parts = NULL;
  zhg->VtxPlan = NULL;
  zhg->Recv_GNOs = NULL;
  zhg->nRecv_GNOs = 0;

  phgraph = &(zhg->PHG);
  Zoltan_HG_HGraph_Init(phgraph);

  /* just set the pointer of phgraph's comm to hgp's comm */
  phgraph->comm = &hgp->globalcomm;

  /* Use callback functions to build the hypergraph. */
  if (zz->Get_Num_HG_Edges && zz->Get_HG_Edge_List && zz->Get_Num_HG_Pins){
    /* 
     * Hypergraph callback functions exist; 
     * call them and build the HG directly.
     */
    ZOLTAN_TRACE_DETAIL(zz, yo, "Using Hypergraph Callbacks.");

    ierr = Zoltan_PHG_Fill_Hypergraph(zz, zhg, input_parts);
    if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
      ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Error building hypergraph");
      goto End;
    }
  }

  else if ((zz->Get_Num_Edges != NULL || zz->Get_Num_Edges_Multi != NULL) &&
           (zz->Get_Edge_List != NULL || zz->Get_Edge_List_Multi != NULL)) {
    /* 
     * Hypergraph callback functions don't exist, but graph functions do;
     * call the graph callback, build a graph, and convert it to a hypergraph. 
     */
    Graph graph;             /* Temporary graph. */

    ZOLTAN_TRACE_DETAIL(zz, yo, "Using Graph Callbacks.");

    ZOLTAN_PRINT_WARN(zz->Proc, yo, "GRAPH TO HGRAPH CONVERSION MAY NOT BE "
                      "CORRECT IN PARALLEL YET -- KDD KDDKDD");

    Zoltan_HG_Graph_Init(&graph);
    ierr = Zoltan_Get_Obj_List(zz, &(graph.nVtx), &(zhg->GIDs),
      &(zhg->LIDs), zz->Obj_Weight_Dim, &(graph.vwgt), &(zhg->Input_Parts));
    if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
      ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Error getting object data");
      Zoltan_HG_Graph_Free(&graph);
      goto End;
    }

    ierr = Zoltan_Build_Graph(zz, 1, hgp->check_graph, graph.nVtx,
     zhg->GIDs, zhg->LIDs, zz->Obj_Weight_Dim, zz->Edge_Weight_Dim,
     &(graph.vtxdist), &(graph.nindex), &(graph.neigh), &(graph.ewgt));
    if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
      ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Error building graph");
      Zoltan_HG_Graph_Free(&graph);
      goto End;
    }
 
    graph.nEdge = graph.nindex[graph.nVtx];
    ierr = Zoltan_HG_Graph_to_HGraph(zz, &graph, phgraph);
    if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
      ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Error converting graph to hypergraph");
      Zoltan_HG_Graph_Free(&graph);
      goto End;
    }

    Zoltan_HG_Graph_Free(&graph);
  }

#ifdef KDDKDD_NO_COORDINATES_FOR_NOW
  /* KDDKDD DON'T KNOW WHAT TO DO WITH THE COORDINATES WITH 2D DISTRIB ANYWAY */
  if (zz->Get_Num_Geom != NULL && 
      (zz->Get_Geom != NULL || zz->Get_Geom_Multi != NULL)) {
     /* Geometric callbacks are registered;       */
     /* get coordinates for hypergraph objects.   */
     ZOLTAN_TRACE_DETAIL(zz, yo, "Getting Coordinates.");
     ierr = Zoltan_Get_Coordinates(zz, phgraph->nVtx, zhg->GIDs,
      zhg->LIDs, &(phgraph->nDim), &(phgraph->coor));
  }
#endif

  if (hgp->check_graph) {
    ierr = Zoltan_HG_Check(zz, phgraph);
    if (ierr == ZOLTAN_WARN) {
      ZOLTAN_PRINT_WARN(zz->Proc, yo, "Warning returned from Zoltan_HG_Check");
    }
    else if (ierr != ZOLTAN_OK) {
      ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Error returned from Zoltan_HG_Check");
      goto End;     
    }
  }


  if (hgp->output_level >= PHG_DEBUG_PLOT)
    Zoltan_PHG_Plot_2D_Distrib(zz, &(zhg->PHG));

  if (hgp->output_level >= PHG_DEBUG_PRINT)
    Zoltan_PHG_HGraph_Print(zz, zhg, &(zhg->PHG), *input_parts, stdout);

End:
  if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
    /* Return NULL zhg */
    Zoltan_HG_HGraph_Free(&(zhg->PHG));
    Zoltan_Multifree(__FILE__, __LINE__, 4, &(zhg->GIDs),
     &(zhg->LIDs), &(zhg->Input_Parts), zoltan_hg);
  }
    
  ZOLTAN_TRACE_EXIT(zz, yo);
  return ierr;
}

/*****************************************************************************/


static int Zoltan_PHG_Fill_Hypergraph(
  ZZ *zz,
  ZPHG *zhg,      /* Description of hypergraph provided by the application. */
  Partition *input_parts   /* Initial partition assignment of vtxs in 
                              2D data distribution; length = zhg->PHG->nVtx. */
)
{
/* Routine to call HG query function and build HG data structure. 
 * Assumes (for now) that input is given as follows from application:
 *  - Application already provided list of objects (vertices) assigned
 *    to proc.  (KDD -- perhaps move that call here).
 *  - Application gives hyperedges it owns; it knows GIDs and processor owner
 *    of all vertices of each owned hyperedge.
 *  - Application gives each hyperedge only once (KDDKDD -- MAY REMOVE THIS
 *    RESTRICTION LATER. KDDKDD)
 * Output is a fully functioning parallel hypergraph with 2D distribution of
 * pins (non-zeros).
 */

char *yo = "Zoltan_PHG_Fill_Hypergraph";
struct application_input {     /* Data provided by application callbacks. */
  int nVtx;                         /* # objects (vertices) on proc. */
  int nEdge;                        /* # hyperedges on proc. */
  int nPins;                        /* # pins (nonzeros) on proc. */
  int GnVtx;                        /* Total nVtx across all procs. */
  int GnEdge;                       /* Total nEdge across all procs. */
  int *edge_sizes;                  /* # of GIDs in each hyperedge. */
  ZOLTAN_ID_PTR pins;               /* Object GIDs (vertices) belonging to 
                                       hyperedges.  */
  int *pin_procs;                   /* Processor owning each pin vertex of 
                                       hyperedges. */
  int *pin_gno;                     /* Global numbers in range [0,GnVtx-1]
                                       for pins. */
  int *vtx_gno;                     /* Global numbers in range [0,GnVtx-1]
                                       for vertices.  app.vtx_gno[i] is
                                       global number for GID[i]. */
  int *edge_gno;                    /* Global numbers in range [0,GnEdge-1]
                                       for edges.  app.edge[i] is
                                       global number for this proc's edge i. */
  int *vtxdist;                     /* Distribution of vertices
                                       across original owning processors;
                                       # vtxs on proc i == 
                                       vtxdist[i+1] - vtxdist[i]. */
                                    /* KDDKDD Needed only for linear initial
                                       2D distribution; can remove if use other
                                       initial distribution. */
  int *edgedist;                    /* Distribution of edges
                                       across original owning processors;
                                       # edges on proc i == 
                                       edgedist[i+1] - edgedist[i]. */
                                    /* KDDKDD Needed only for linear initial
                                       2D distribution; can remove if use other
                                       initial distribution. */
  float *vwgt;                      /* Vertex weights. */
  float *ewgt;                      /* Edge weights. */
} app;

struct Hash_Node *hash_nodes = NULL;  /* Hash table variables for mapping   */
struct Hash_Node **hash_tab = NULL;   /* GIDs to global numbering system.   */
ZOLTAN_COMM_OBJ *plan;

int i, j, cnt, dim;
int msg_tag = 30000;
int ierr = ZOLTAN_OK;
int nProc = zz->Num_Proc;
int nRequests;
ZOLTAN_ID_PTR pin_requests = NULL;
int *request_gno = NULL;
int edge_gno, edge_Proc_y;
int vtx_gno, vtx_Proc_x;
int *proclist = NULL;
int *sendbuf = NULL;
int nnz, idx;
int *nonzeros = NULL;
int *tmp = NULL;
int *hindex = NULL, *hvertex = NULL;
int *dist_x = NULL, *dist_y = NULL;
int nEdge, nVtx, nwgt = 0;
int nrecv, *recv_gno = NULL; 
int *tmpparts = NULL, *recvparts = NULL;

ZOLTAN_ID_PTR global_ids;
int num_gid_entries = zz->Num_GID;
HGraph *phg = &(zhg->PHG);

int nProc_x = phg->comm->nProc_x;
int nProc_y = phg->comm->nProc_y;
int myProc_x = phg->comm->myProc_x;
int myProc_y = phg->comm->myProc_y;

float frac_x, frac_y;
float *tmpwgts = NULL;

  ZOLTAN_TRACE_ENTER(zz, yo);

  /**************************************************/
  /* Obtain vertex information from the application */
  /**************************************************/

  app.edge_sizes = NULL;
  app.pins = NULL;
  app.pin_procs = NULL;
  app.pin_gno = NULL;
  app.vtxdist = NULL;
  app.vtx_gno = NULL;
  app.edgedist = NULL;
  app.edge_gno = NULL;
  app.vwgt = NULL;
  app.ewgt = NULL;

  ierr = Zoltan_Get_Obj_List(zz, &(zhg->nObj), &(zhg->GIDs), &(zhg->LIDs), 
                             zz->Obj_Weight_Dim, &app.vwgt, 
                             &(zhg->Input_Parts));
  if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
    ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Error getting object data");
    goto End;
  }

  app.nVtx = zhg->nObj;

  /* Build app.vtxdist as in Zoltan_Build_Graph. */

  app.vtxdist = (int *)ZOLTAN_MALLOC(2 * (nProc+1) * sizeof(int));
  if (!(app.vtxdist)) MEMORY_ERROR;

  app.edgedist = app.vtxdist + nProc + 1;

  /* Construct app.vtxdist[i] = the number of vertices on all procs < i. */
  /* Scan to compute partial sums of the number of objs */

  MPI_Scan (&app.nVtx, app.vtxdist, 1, MPI_INT, MPI_SUM, zz->Communicator);

  /* Gather data from all procs */

  MPI_Allgather (&(app.vtxdist[0]), 1, MPI_INT,
                 &(app.vtxdist[1]), 1, MPI_INT, zz->Communicator);
  app.vtxdist[0] = 0;
  app.GnVtx = app.vtxdist[nProc];

  /* 
   * Correlate GIDs in edge_verts with local indexing in zhg to build the
   * input HG.
   * Use hash table to map global IDs to local position in zhg->GIDs.
   * Based on hashing code in Zoltan_Build_Graph.
   * KDD -- This approach is serial for now; look more closely at 
   * KDD -- Zoltan_Build_Graph when move to parallel.
   */

  /* Construct local hash table mapping GIDs to global number (gno) */
  if (app.nVtx) {
    hash_nodes = (struct Hash_Node *) ZOLTAN_MALLOC(app.nVtx * 
                                                    sizeof(struct Hash_Node));
    hash_tab = (struct Hash_Node **) ZOLTAN_MALLOC(app.nVtx *
                                                   sizeof(struct Hash_Node *));
    app.vtx_gno = (int *) ZOLTAN_MALLOC(app.nVtx * sizeof(int));
    if (!hash_nodes || !hash_tab || !app.vtx_gno) MEMORY_ERROR;

    global_ids = zhg->GIDs;

    /* Assign consecutive numbers based on the order of the ids */
    /* KDDKDD  For different (e.g., randomized) initial 2D distributions, 
     * KDDKDD  change the way vtx_gno values are assigned. */

    for (i=0; i< app.nVtx; i++) {
      hash_tab[i] = NULL;
      hash_nodes[i].gid = &(global_ids[i*num_gid_entries]);
      hash_nodes[i].gno = app.vtx_gno[i] =  app.vtxdist[zz->Proc]+i;
    }

    for (i=0; i< app.nVtx; i++){
      /* insert hashed elements into hash table */
      j = Zoltan_Hash(&(global_ids[i*num_gid_entries]), num_gid_entries,
                      (unsigned int) app.nVtx);
      hash_nodes[i].next = hash_tab[j];
      hash_tab[j] = &hash_nodes[i];
    }
  }

  /***********************************************************************/
  /* Get hyperedge information from application through query functions. */
  /***********************************************************************/

  app.nEdge = zz->Get_Num_HG_Edges(zz->Get_Num_HG_Edges_Data, &ierr);
  if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
    ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Error returned from Get_Num_HG_Edges");
    goto End;
  }
  
  /* KDD:  question:  How do we compute size to malloc array for HG Edges? 
   * KDD:  We can't have a size function unless we assume the application
   * KDD:  can "name" the hyperedges.
   * KDD:  For now, assume application can return number of pins.
   */

  app.nPins = zz->Get_Num_HG_Pins(zz->Get_Num_HG_Pins_Data, &ierr);
  if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
    ZOLTAN_PRINT_ERROR(zz->Proc, yo,
                       "Error returned from Get_Max_HG_Edge_Size");
    goto End;
  }

  if (app.nEdge > 0) {
    nwgt = app.nEdge * zz->Edge_Weight_Dim;
    app.pins = ZOLTAN_MALLOC_GID_ARRAY(zz, app.nPins);
    app.edge_sizes = (int *) ZOLTAN_MALLOC(app.nEdge * sizeof(int));
    app.pin_procs = (int *) ZOLTAN_MALLOC(app.nPins * sizeof(int));
    app.pin_gno = (int *) ZOLTAN_MALLOC(app.nPins * sizeof(int));
    app.edge_gno = (int *) ZOLTAN_MALLOC(app.nEdge * sizeof(int));
    if (nwgt) 
      app.ewgt = (float *) ZOLTAN_MALLOC(nwgt * sizeof(float));
    if (!app.pins || !app.edge_sizes || !app.pin_procs || !app.pin_gno  ||
        !app.edge_gno || (nwgt && !app.ewgt)) MEMORY_ERROR;

    ierr = zz->Get_HG_Edge_List(zz->Get_HG_Edge_List_Data, num_gid_entries,
                                zz->Edge_Weight_Dim, app.nEdge, app.nPins,
                                app.edge_sizes, app.pins, 
                                app.pin_procs, app.ewgt);
    if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
      ZOLTAN_PRINT_ERROR(zz->Proc, yo,"Error returned from Get_HG_Edge_List");
      goto End;
    }
  }
  
  /* 
   * KDDKDD -- Assuming hyperedges are given to Zoltan by one processor only.
   * KDDKDD -- Eventually, will loosen that constraint and remove duplicates.
   * KDDKDD -- Or the code might work (although with extra communication)
   * KDDKDD -- with the duplicates.
   * KDDKDD -- Might be easier once we have edge GIDs.
   */
  /* Impose a global hyperedge numbering */
  /* Construct app.edgedist[i] = the number of edges on all procs < i. */
  /* Scan to compute partial sums of the number of edges */

  MPI_Scan (&app.nEdge, app.edgedist, 1, MPI_INT, MPI_SUM, zz->Communicator);

  /* Gather data from all procs */

  MPI_Allgather (&(app.edgedist[0]), 1, MPI_INT,
                 &(app.edgedist[1]), 1, MPI_INT, zz->Communicator);
  app.edgedist[0] = 0;
  app.GnEdge = app.edgedist[nProc];

  /* Assign global numbers to edges. */
  /* KDDKDD  For different (e.g., randomized) initial 2D distributions, 
   * KDDKDD  change the way edge_gno values are assigned. */
  for (i = 0; i < app.nEdge; i++)
    app.edge_gno[i] = app.edgedist[zz->Proc] + i;
   
  /* 
   * Obtain the global num in range 0 .. (total_num_vtx-1) 
   * for each vertex pin.
   * For each pin GID in app.pins, request the global number (gno) from the
   * input processor.
   * Fill requests (using hash table) for GIDs local to this processor.
   * Upon completion, app.pin_gno will contain the global nums.
   */

  ierr = Zoltan_Comm_Create(&plan, app.nPins, app.pin_procs, zz->Communicator,
                            msg_tag, &nRequests);

  if (nRequests) {
    pin_requests = ZOLTAN_MALLOC_GID_ARRAY(zz, nRequests);
    request_gno = (int *) ZOLTAN_MALLOC(nRequests * sizeof(int));
    if (!pin_requests || !request_gno)
      MEMORY_ERROR;
  }

  msg_tag--;
  ierr = Zoltan_Comm_Do(plan, msg_tag, (char *) app.pins, 
                        sizeof(ZOLTAN_ID_TYPE) * num_gid_entries,
                        (char *) pin_requests);

  for (i = 0; i < nRequests; i++)
    request_gno[i] = hash_lookup(zz, &(pin_requests[i*num_gid_entries]),
                                 app.nVtx, hash_tab);

  msg_tag--;
  Zoltan_Comm_Do_Reverse(plan, msg_tag, (char *) request_gno, sizeof(int), NULL,
                         (char *) app.pin_gno);

  Zoltan_Comm_Destroy(&plan);

  /* 
   * Compute the distribution of vertices and edges to the 2D data
   * distribution's processor columns and rows.
   * For now, these distributions are described by arrays dist_x
   * and dist_y; in the future, we may prefer a hashing function
   * mapping GIDs to processor columns and rows. KDDKDD
   */

  phg->dist_x = dist_x = (int *) ZOLTAN_CALLOC((nProc_x+1), sizeof(int));
  phg->dist_y = dist_y = (int *) ZOLTAN_CALLOC((nProc_y+1), sizeof(int));

  if (!dist_x || !dist_y) MEMORY_ERROR;

  frac_x = (float) app.GnVtx / (float) nProc_x;
  for (i = 1; i < nProc_x; i++)
    dist_x[i] = (int) (i * frac_x);
  dist_x[nProc_x] = app.GnVtx;
  
  frac_y = (float) app.GnEdge / (float) nProc_y;
  for (i = 1; i < nProc_y; i++)
    dist_y[i] = (int) (i * frac_y);
  dist_y[nProc_y] = app.GnEdge;
  
  nEdge = dist_y[myProc_y+1] - dist_y[myProc_y];
  nVtx = dist_x[myProc_x+1] - dist_x[myProc_x];

  /*
   * Build comm plan for sending non-zeros to their target processors in
   * 2D data distribution. 
   */

  proclist = (int *) ZOLTAN_MALLOC(MAX(app.nPins,app.nVtx) * sizeof(int));
  sendbuf = (int *) ZOLTAN_MALLOC(app.nPins * 2 * sizeof(int));

  cnt = 0; 
  for (i = 0; i < app.nEdge; i++) {
    /* processor row for the edge */
    edge_gno = app.edge_gno[i];
    edge_Proc_y = EDGE_TO_PROC_Y(phg, edge_gno);

    for (j = 0; j < app.edge_sizes[i]; j++) {
      /* processor column for the vertex */
      vtx_gno = app.pin_gno[cnt];
      vtx_Proc_x = VTX_TO_PROC_X(phg, vtx_gno);

      proclist[cnt] = edge_Proc_y * nProc_x + vtx_Proc_x;
      sendbuf[2*cnt] = edge_gno;
      sendbuf[2*cnt+1] = vtx_gno;
      cnt++;
    } 
  }

  /*
   * Send pins to their target processors.
   * They become non-zeros in the 2D data distribution.
   */

  msg_tag--;
  ierr = Zoltan_Comm_Create(&plan, cnt, proclist, zz->Communicator,
                     msg_tag, &nnz);

  if (nnz) {
    nonzeros = (int *) ZOLTAN_MALLOC(nnz * 2 * sizeof(int));
    if (!nonzeros) MEMORY_ERROR;
  }

  msg_tag--;
  Zoltan_Comm_Do(plan, msg_tag, (char *) sendbuf, 2*sizeof(int),
                 (char *) nonzeros);

  Zoltan_Comm_Destroy(&plan);

  /* Unpack the non-zeros received. */

  tmp = (int *) ZOLTAN_CALLOC(nEdge + 1, sizeof(int));
  hindex = (int *) ZOLTAN_CALLOC(nEdge + 1, sizeof(int));
  hvertex = (int *) ZOLTAN_MALLOC(nnz * sizeof(int));

  if (!tmp || !hindex || (nnz && !hvertex)) MEMORY_ERROR;

  /* Count the number of nonzeros per hyperedge */
  for (i = 0; i < nnz; i++) {
    idx = EDGE_GNO_TO_LNO(phg, nonzeros[2*i]); 
    tmp[idx]++;
  }

  /* Compute prefix sum to represent hindex correctly. */
  for (i = 0; i < nEdge; i++)  {
    hindex[i+1] = hindex[i] + tmp[i];
    tmp[i] = 0;
  }
       
  for (i = 0; i < nnz; i++) {
    idx = EDGE_GNO_TO_LNO(phg, nonzeros[2*i]);
    hvertex[hindex[idx] + tmp[idx]] = VTX_GNO_TO_LNO(phg, nonzeros[2*i+1]);
    tmp[idx]++;
  }


  phg->nVtx = nVtx;
  phg->nEdge = nEdge;
  phg->nPins = nnz;
  phg->hindex = hindex;
  phg->hvertex = hvertex;

  ierr = Zoltan_HG_Create_Mirror(zz, phg);
  if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
    ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Error from Zoltan_HG_Create_Mirror");
    goto End;
  }

  /* Send vertex partition assignments and weights, if any. */
  /* Can use same plan for both. */

  if (phg->comm->nProc_x > 1) {

    /* Need a communication plan mapping GIDs to their GNOs processors
     * within a row communicator.  The plan is used to send vertex weights
     * and partition assignments to the 2D distribution and/or 
     * to create return lists after partitioning
     */

    for (i = 0; i < app.nVtx; i++)
      proclist[i] = VTX_TO_PROC_X(phg, app.vtx_gno[i]);
      
    msg_tag++;
    ierr = Zoltan_Comm_Create(&(zhg->VtxPlan), app.nVtx, proclist, 
                              phg->comm->row_comm, msg_tag, &nrecv);
    zhg->nRecv_GNOs = nrecv;

    zhg->Recv_GNOs = recv_gno = (int *) ZOLTAN_MALLOC(nrecv * sizeof(int));
    if (nrecv && !recv_gno) MEMORY_ERROR;

    /* Use plan to send weights to the appropriate proc_x. */
    msg_tag++;
    ierr = Zoltan_Comm_Do(zhg->VtxPlan, msg_tag, (char *) app.vtx_gno, 
                          sizeof(int), (char *) recv_gno);

  }
  else {
    /* Save map of what needed. */
    zhg->nRecv_GNOs = nrecv = app.nVtx;
    zhg->Recv_GNOs = recv_gno = app.vtx_gno;
  }

  /* Send vertex partition assignments to 2D distribution. */

  tmpparts = (int *) ZOLTAN_CALLOC(phg->nVtx, sizeof(int));
  *input_parts = (int *) ZOLTAN_MALLOC(phg->nVtx * sizeof(int));
  if (phg->nVtx && (!tmpparts || !*input_parts)) MEMORY_ERROR;

  if (phg->comm->nProc_x == 1)  {
    for (i = 0; i < app.nVtx; i++) {
      idx = app.vtx_gno[i];
      tmpparts[idx] = zhg->Input_Parts[i];
    }
  }
  else {
    /* In using input_parts for recv, assuming nrecv <= phg->nVtx */
    msg_tag++;
    ierr = Zoltan_Comm_Do(zhg->VtxPlan, msg_tag, (char *) zhg->Input_Parts,
                          sizeof(int), (char *) *input_parts);

    for (i = 0; i < nrecv; i++) {
      idx = VTX_GNO_TO_LNO(phg, recv_gno[i]);
      tmpparts[idx] = (*input_parts)[i];
    }
  }

  /* Reduce partition assignments for all vertices within column 
   * to all processors within column.
   */

  MPI_Allreduce(tmpparts, *input_parts, phg->nVtx, MPI_INT, MPI_MAX, 
                phg->comm->col_comm);

  ZOLTAN_FREE(&tmpparts);
  

  /* Allocate temp storage for vtx and/or edge weights. */

  if (zz->Obj_Weight_Dim || zz->Edge_Weight_Dim) {
    nwgt = MAX(phg->nVtx * zz->Obj_Weight_Dim, 
               phg->nEdge * zz->Edge_Weight_Dim);
    tmpwgts = (float *) ZOLTAN_MALLOC(nwgt * sizeof(float));
    if (nwgt && !tmpwgts) MEMORY_ERROR;
  }

  /* Send vertex weights to 2D distribution. */

  if (zz->Obj_Weight_Dim) {
    dim = phg->VtxWeightDim = zz->Obj_Weight_Dim;
    for (i = 0; i < phg->nVtx; i++) tmpwgts[i] = 0;
    nwgt = phg->nVtx * dim;
    phg->vwgt = (float *) ZOLTAN_CALLOC(nwgt, sizeof(float));
    if (nwgt && !phg->vwgt) 
      MEMORY_ERROR;

    if (phg->comm->nProc_x == 1)  {
      for (i = 0; i < app.nVtx; i++) {
        idx = app.vtx_gno[i];
        for (j = 0; j < dim; j++)
          tmpwgts[idx * dim + j] = app.vwgt[i * dim + j];
      }
    }
    else {
      
      /* In using phg->vwgt for recv, assuming nrecv <= phg->nVtx */
      msg_tag++;
      ierr = Zoltan_Comm_Do(zhg->VtxPlan, msg_tag, (char *) app.vwgt, 
                            dim*sizeof(float), (char *) phg->vwgt);
      
      for (i = 0; i < nrecv; i++) {
        idx = VTX_GNO_TO_LNO(phg, recv_gno[i]);
        for (j = 0; j < dim; j++) 
          tmpwgts[idx * dim + j] = phg->vwgt[i * dim + j];
      }
    }

    /* Reduce weights for all vertices within column 
     * to all processors within column.
     */

    MPI_Allreduce(tmpwgts, phg->vwgt, nwgt, MPI_FLOAT, MPI_MAX, 
                  phg->comm->col_comm);
  }
  else {
    /* Application did not specify object weights, but PHG code needs them.
     * Create uniform weights.
     */
    phg->VtxWeightDim = 1;
    phg->vwgt = (float *) ZOLTAN_MALLOC(phg->nVtx * sizeof(float));
    for (i = 0; i < phg->nVtx; i++)
      phg->vwgt[i] = 1.;
  }

  if (zz->LB.Return_Lists == ZOLTAN_LB_NO_LISTS) {
    /* Don't need the plan long-term; destroy it now. */
    Zoltan_Comm_Destroy(&(zhg->VtxPlan));
    if (zhg->Recv_GNOs == app.vtx_gno) app.vtx_gno = NULL;
    ZOLTAN_FREE(&(zhg->Recv_GNOs));
    zhg->nRecv_GNOs = 0;
  }

  /*  Send edge weights, if any */

  if (zz->Edge_Weight_Dim) {
    dim = phg->EdgeWeightDim = zz->Edge_Weight_Dim;
    for (i = 0; i < phg->nEdge; i++) tmpwgts[i] = 0;
    nwgt = phg->nEdge * dim;
    phg->ewgt = (float *) ZOLTAN_CALLOC(nwgt, sizeof(float));
    if (nwgt && !phg->ewgt)
      MEMORY_ERROR;

    if (phg->comm->nProc_y == 1) {
      for (i = 0; i < app.nEdge; i++) {
        idx = app.edge_gno[i];
        for (j = 0; j < dim; j++)
          tmpwgts[idx * dim + j] = app.ewgt[i*dim + j];
      }
    }
    else {
      for (i = 0; i < app.nEdge; i++)
        proclist[i] = EDGE_TO_PROC_Y(phg, app.edge_gno[i]);
      
      msg_tag++;
      ierr = Zoltan_Comm_Create(&plan, app.nEdge, proclist, 
                                phg->comm->col_comm, msg_tag, &nrecv);

      recv_gno = (int *) ZOLTAN_MALLOC(nrecv * sizeof(int));
      if (nrecv && !recv_gno) MEMORY_ERROR;

      msg_tag++;
      ierr = Zoltan_Comm_Do(plan, msg_tag, (char *) app.edge_gno, sizeof(int),
                            (char *) recv_gno);

      /* In using phg->vwgt for recv, assuming nrecv <= phg->nVtx */
      msg_tag++;
      ierr = Zoltan_Comm_Do(plan, msg_tag, (char *) app.ewgt, 
                            dim*sizeof(float), (char *) phg->ewgt);

      Zoltan_Comm_Destroy(&plan);

      for (i = 0; i < nrecv; i++) {
        idx = EDGE_GNO_TO_LNO(phg, recv_gno[i]);
        for (j = 0; j < dim; j++) 
          tmpwgts[idx * dim + j] = phg->ewgt[i * dim + j];
      }
      ZOLTAN_FREE(&recv_gno); 
    }

    /* Need to gather weights for all edges within row 
     * to all processors within row.
     */

    MPI_Allreduce(tmpwgts, phg->ewgt, nwgt, MPI_FLOAT, MPI_MAX, 
                  phg->comm->row_comm);
  }
  else {
    /* KDDKDD  For now, do not assume uniform edge weights.
     * KDDKDD  Can add later if, e.g., we decide to coalesce identical edges.
     */
    phg->EdgeWeightDim = 0;
  }

End:
  if (ierr != ZOLTAN_OK && ierr != ZOLTAN_WARN) {
    Zoltan_HG_HGraph_Free(phg);
  }
  
  Zoltan_Multifree(__FILE__, __LINE__, 10,  &app.pins, 
                                            &app.edge_sizes, 
                                            &app.pin_procs, 
                                            &app.pin_gno, 
                                            &app.vwgt,
                                            &app.ewgt,
                                            &app.vtxdist,
                                            &app.edge_gno,
                                            &hash_nodes,
                                            &hash_tab);
  if (zhg->Recv_GNOs != app.vtx_gno) 
    ZOLTAN_FREE(&app.vtx_gno);
  ZOLTAN_FREE(&tmp);
  ZOLTAN_FREE(&tmpwgts);
  ZOLTAN_FREE(&nonzeros);
  ZOLTAN_FREE(&proclist);
  ZOLTAN_FREE(&sendbuf);
  ZOLTAN_FREE(&pin_requests);
  ZOLTAN_FREE(&request_gno);

  ZOLTAN_TRACE_EXIT(zz, yo);
  return ierr;
}

/*****************************************************************************/

static int hash_lookup(
  ZZ *zz,
  ZOLTAN_ID_PTR key,
  int nVtx,
  struct Hash_Node **hash_tab
)
{
/* Looks up a key GID in the hash table; returns its gno. */
/* Based on hash_lookup in build_graph.c. */

  int i;
  struct Hash_Node *ptr;

  i = Zoltan_Hash(key, zz->Num_GID, (unsigned int) nVtx);
  for (ptr=hash_tab[i]; ptr != NULL; ptr = ptr->next){
    if (ZOLTAN_EQ_GID(zz, ptr->gid, key))
      return (ptr->gno);
  }
  /* Key not in hash table */
  return -1;
}


#ifdef __cplusplus
} /* closing bracket for extern "C" */
#endif
