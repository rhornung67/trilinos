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

#include "phg.h"
#include "phg_distrib.h"




#define _DEBUG1
#define _DEBUG2
#define _DEBUG3


#define MEMORY_ERROR { \
  ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Memory error."); \
  ierr = ZOLTAN_MEMERR; \
  goto End; \
}


static int split_hypergraph (int *pins[2], HGraph*, HGraph*, Partition, int, ZZ*);



/* recursively divides problem into 2 parts until all p found */
int Zoltan_PHG_rdivide(int lo, int hi, Partition final, ZZ *zz, HGraph *hg,
                       PHGPartParams *hgp, int level)
{
  char *yo = "Zoltan_PHG_rdivide";
  int i, j, mid, ierr=ZOLTAN_OK, *pins[2] = {NULL,NULL}, *lpins[2] = {NULL,NULL};
  Partition part=NULL;
  HGraph *left=NULL, *right=NULL;
  int    *proclist=NULL, *sendbuf=NULL, *recvbuf=NULL, nsend, msg_tag=7777;
  PHGComm *hgc = hg->comm;
  float tgpartsize[2]={0.0,0.0};    /* Target partition sizes; dimension is 2 
                                     because we are doing bisection */

  hg->redl = hgp->redl;
  
  /* only one part remaining, record results and exit */
  if (lo == hi) {
    for (i = 0; i < hg->nVtx; ++i)
      final[hg->vmap[i]] = lo;
    return ZOLTAN_OK;
  }

  if (hg->nVtx && !(part = (Partition) ZOLTAN_MALLOC (hg->nVtx * sizeof (int))))
      MEMORY_ERROR;

  /* bipartition current hypergraph with appropriate split ratio */
  mid = (lo+hi)/2;
  tgpartsize[0] = tgpartsize[1] = 0.;
  for (i = lo; i <= mid; i++)  tgpartsize[0] += hgp->part_sizes[i];
  for (i = lo; i <= hi;  i++)  tgpartsize[1] += hgp->part_sizes[i];
  hg->ratio = (double) tgpartsize[0] / (double) tgpartsize[1];
  tgpartsize[0] = hg->ratio;
  tgpartsize[1] = 1. - tgpartsize[0];

  ierr = Zoltan_PHG_Partition (zz, hg, 2, tgpartsize, part, hgp, level);
  if (ierr != ZOLTAN_OK)
      goto End;

  uprintf(hgc, "Rdivide(%d, %d): %.1lf\n", lo, hi, Zoltan_PHG_hcut_size_links(hgc, hg, part, 2));
    
  /* if only two parts total, record results and exit */
  if (lo + 1 == hi)  {
    for (i = 0; i < hg->nVtx; ++i)
      final[hg->vmap[i]] = ((part[i] == 0) ? lo : hi);
    ZOLTAN_FREE (&part);
    return ZOLTAN_OK;
  }

  if (hg->nEdge && (!(pins[0] = (int*) ZOLTAN_CALLOC (2 * hg->nEdge, sizeof(int)))
   || !(lpins[0] = (int*) ZOLTAN_CALLOC (2 * hg->nEdge, sizeof(int)))))
      MEMORY_ERROR;
  if (pins[0] && lpins[0]) {
      pins[1]  = &( pins[0][hg->nEdge]);
      lpins[1] = &(lpins[0][hg->nEdge]);
  }
     
  /* Initial calculation of the local pin distribution  (sigma in UVC's papers)  */
  for (i = 0; i < hg->nEdge; ++i)
      for (j = hg->hindex[i]; j < hg->hindex[i+1]; ++j)
        ++(lpins[part[hg->hvertex[j]]][i]);
        
  /* now compute global pin distribution */
  MPI_Allreduce(lpins[0], pins[0], 2*hg->nEdge, MPI_INT, MPI_SUM, hgc->row_comm);
  ZOLTAN_FREE (&lpins[0]);                        /* we don't need lpins */
    
  /* recursively divide in two parts and repartition hypergraph */
  if (mid>lo) { /* only split if we really need it */
      if (!(left = (HGraph*) ZOLTAN_MALLOC (sizeof (HGraph))))
          MEMORY_ERROR;
      
      ierr = split_hypergraph (pins, hg, left, part, 0, zz);
      if (ierr != ZOLTAN_OK) 
          goto End;
  } else {
      for (i = 0; i < hg->nVtx; ++i)
          if (part[i]==0)
              final[hg->vmap[i]] = lo;
  }

  if (hi>mid+1) { /* only split if we need it */
      if (!(right = (HGraph*) ZOLTAN_MALLOC (sizeof (HGraph))))
          MEMORY_ERROR;
      ierr = split_hypergraph (pins, hg, right, part, 1, zz);
  
      ZOLTAN_FREE (&pins[0]); /* we don't need pins */      
      if (ierr != ZOLTAN_OK)
          goto End;
  } else {
      ZOLTAN_FREE (&pins[0]); /* we don't need pins */
      for (i = 0; i < hg->nVtx; ++i)
          if (part[i]==1)
              final[hg->vmap[i]] = hi;
  }


  if (hgp->proc_split && hgc->nProc>1 && left && right) {
      PHGComm  leftcomm, rightcomm;
      HGraph  newleft, newright;
      int     *leftvmap=NULL, *rightvmap=NULL, *leftdest=NULL, *rightdest=NULL, procmid;
      ZOLTAN_COMM_OBJ *plan=NULL;    

      /* redistribute left and right parts */
      procmid = (int)((float) (hgc->nProc-1) * (float) left->dist_x[hgc->nProc_x] / (float) hg->dist_x[hgc->nProc_x]);
#ifdef _DEBUG1
      if (procmid<0 || (procmid+1>hgc->nProc-1))
          errexit("hey hey Proc Number range is [0, %d] prcomid=%d for left #pins=%d nPins=%d", hgc->nProc-1, procmid, left->dist_x[hgc->nProc_x] , hg->dist_x[hgc->nProc_x]);
#endif
      uprintf(hgc, "before redistribute for left procmid=%d ------------------\n", procmid);
      Zoltan_PHG_Redistribute(zz, left,
                              0, procmid, 
                              &leftcomm, 
                              &newleft,
                              &leftvmap, &leftdest);
      Zoltan_HG_HGraph_Free (left);
      
      uprintf(hgc, "before redistribute for right ++++++++++++++++++++++\n");
      Zoltan_PHG_Redistribute(zz, right,
                              procmid+1, hgc->nProc-1, 
                              &rightcomm, 
                              &newright,
                              &rightvmap, &rightdest);
      Zoltan_HG_HGraph_Free (right);
      
      nsend = MAX(newleft.nVtx, newright.nVtx);
      proclist = (int *) ZOLTAN_MALLOC (nsend * sizeof (int));
      sendbuf =  (int *) ZOLTAN_MALLOC (nsend * 2 * sizeof (int));
      recvbuf =  (int *) ZOLTAN_MALLOC (hg->nVtx * 2 * sizeof (int));
      if ((nsend && (!proclist || !sendbuf)) ||
          (hg->nVtx && !recvbuf))
          MEMORY_ERROR;
      nsend = 0;
      if (hgc->myProc<=procmid) {/* I'm on the left part so I should partition newleft */

          uprintf(hgc, "-----------------I'm partitioning left---------------\n");
          Zoltan_HG_Check(zz, &newleft);
          // ierr = Zoltan_PHG_rdivide (lo, mid, part, zz, &newleft, hgp, level+1);
#if 0          
          for (i=0; i<newleft.nVtx; ++i) {
              proclist[nsend] = leftdest[i];
              sendbuf[nsend*2] = leftvmap[i];
              sendbuf[nsend*2+1] = part[i];
              ++nsend;
          }

          Zoltan_HG_HGraph_Free (&newleft);
          ZOLTAN_FREE(&leftvmap);
          ZOLTAN_FREE(&leftdest);
          if (leftcomm.col_comm==MPI_COMM_NULL || leftcomm.row_comm==MPI_COMM_NULL || leftcomm.Communicator==MPI_COMM_NULL)
              errexit("hey comm is NULL com=%x col=%x row=%x", leftcomm.Communicator, leftcomm.col_comm, leftcomm.row_comm);
          MPI_Comm_free(&leftcomm.col_comm);
          MPI_Comm_free(&leftcomm.row_comm);
          MPI_Comm_free(&leftcomm.Communicator);
#endif
      } else { /* I'm on the right part so I should partition newright */
          uprintf(hgc, "*****************I'm partitioning right****************\n");          
          Zoltan_HG_Check(zz, &newright);
          ierr |= Zoltan_PHG_rdivide (mid+1, hi, part, zz, &newright, hgp, level+1);
#if 0          
          for (i=0; i<newright.nVtx; ++i) {
              proclist[nsend] = rightdest[i];
              sendbuf[nsend*2]   = rightvmap[i];
              sendbuf[nsend*2+1] = part[i];
              ++nsend;
          }

          Zoltan_HG_HGraph_Free (&newright);
          ZOLTAN_FREE(&rightvmap);
          ZOLTAN_FREE(&rightdest);
          if (rightcomm.col_comm==MPI_COMM_NULL || rightcomm.row_comm==MPI_COMM_NULL || rightcomm.Communicator==MPI_COMM_NULL)
              errexit("hey comm is NULL com=%x col=%x row=%x", rightcomm.Communicator, rightcomm.col_comm, rightcomm.row_comm);
          MPI_Comm_free(&rightcomm.col_comm);
          MPI_Comm_free(&rightcomm.row_comm);
          MPI_Comm_free(&rightcomm.Communicator);
#endif
      }

#if 1
      for (i=0; i<hg->nVtx; ++i)
          final[hg->vmap[i]] = lo + (i % (hi-lo+1));
#else
      --msg_tag;
      ierr |= Zoltan_Comm_Create(&plan, nsend, proclist, hgc->row_comm,
                                 msg_tag, &i);

#ifdef _DEBUG1
      if (!hgc->myProc_y) {
          if (i!=hg->nVtx) 
              errexit("I should be receiving nVtx(%d) part info but received %d", hg->nVtx, i);          
      } else {
          if (i)
              errexit("I'm not in the first row; why I'm receiving %d vertices?", i);
      }
#endif
      
      --msg_tag;
      Zoltan_Comm_Do(plan, msg_tag, (char *) sendbuf, 2*sizeof(int),
                     (char *) recvbuf);

      MPI_Bcast(recvbuf, hg->nVtx*2, MPI_INT, 0, hgc->col_comm);

      for (i=0; i<hg->nVtx; ++i) {
#ifdef _DEBUG1
          int p=recvbuf[i*2+1];
          int v=recvbuf[i*2];

          if (v<0 || v>hg->nVtx)
              errexit("sanity check failed for v=%d nVtx=%d\n", v, hg->nVtx);
          if (p<lo || p>hi)
              errexit("sanity check failed for v=%d p=%d lo=%d hi=%d\n", v, p, lo, hi);
#endif
          final[recvbuf[i*2]] = recvbuf[i*2+1];
      }
      

      Zoltan_Comm_Destroy(&plan);
#endif
  } else {
      if (left) {
          ierr = Zoltan_PHG_rdivide (lo, mid, final, zz, left, hgp, level+1);
          Zoltan_HG_HGraph_Free (left);
      }
      if (right) {
          ierr |= Zoltan_PHG_rdivide (mid+1, hi, final, zz, right, hgp, level+1);
          Zoltan_HG_HGraph_Free (right);
      }
  }
  
 End:
  Zoltan_Multifree (__FILE__, __LINE__, 8, &pins[0], &lpins[0], &part, &left, &right,
                    &proclist, &sendbuf, &recvbuf);

  return ierr;
}



/* recursively divides problem into 2 parts until all p found */
int Zoltan_PHG_rdivide_NoProcSplit(int lo, int hi, Partition final, ZZ *zz, HGraph *hg,
                       PHGPartParams *hgp, int level)
{
  char *yo = "Zoltan_PHG_rdivide";
  int i, j, mid, ierr=ZOLTAN_OK, *pins[2] = {NULL,NULL}, *lpins[2] = {NULL,NULL};
  Partition part=NULL;
  HGraph *left=NULL, *right=NULL;
  PHGComm *hgc = hg->comm;
  float tgpartsize[2]={0.0,0.0};    /* Target partition sizes; dimension is 2 
                                     because we are doing bisection */

  hg->redl = hgp->redl;
  
  /* only one part remaining, record results and exit */
  if (lo == hi) {
    for (i = 0; i < hg->nVtx; ++i)
      final [hg->vmap[i]] = lo;
    return ZOLTAN_OK;
  }

  if (hg->nVtx && !(part = (Partition) ZOLTAN_MALLOC (hg->nVtx * sizeof (int))))
      MEMORY_ERROR;

  /* bipartition current hypergraph with appropriate split ratio */
  mid = (lo+hi)/2;
  tgpartsize[0] = tgpartsize[1] = 0.;
  for (i = lo; i <= mid; i++)  tgpartsize[0] += hgp->part_sizes[i];
  for (i = lo; i <= hi;  i++)  tgpartsize[1] += hgp->part_sizes[i];
  hg->ratio = (double) tgpartsize[0] / (double) tgpartsize[1];
  tgpartsize[0] = hg->ratio;
  tgpartsize[1] = 1. - tgpartsize[0];

  ierr = Zoltan_PHG_Partition (zz, hg, 2, tgpartsize, part, hgp, level);
  if (ierr != ZOLTAN_OK)
      goto End;

  uprintf(hgc, "Rdivide(%d, %d): %.1lf\n", lo, hi, Zoltan_PHG_hcut_size_links(hgc, hg, part, 2));
    
  /* if only two parts total, record results and exit */
  if (lo + 1 == hi)  {
    for (i = 0; i < hg->nVtx; ++i)
      final [hg->vmap[i]] = ((part[i] == 0) ? lo : hi);
    ZOLTAN_FREE (&part);
    return ZOLTAN_OK;
  }

  if (hg->nEdge && (!(pins[0] = (int*) ZOLTAN_CALLOC (2 * hg->nEdge, sizeof(int)))
   || !(lpins[0] = (int*) ZOLTAN_CALLOC (2 * hg->nEdge, sizeof(int)))))
      MEMORY_ERROR;
  if (pins[0] && lpins[0]) {
      pins[1]  = &( pins[0][hg->nEdge]);
      lpins[1] = &(lpins[0][hg->nEdge]);
  }
     
  /* Initial calculation of the local pin distribution  (sigma in UVC's papers)  */
  for (i = 0; i < hg->nEdge; ++i)
      for (j = hg->hindex[i]; j < hg->hindex[i+1]; ++j)
        ++(lpins[part[hg->hvertex[j]]][i]);
        
  /* now compute global pin distribution */
  MPI_Allreduce(lpins[0], pins[0], 2*hg->nEdge, MPI_INT, MPI_SUM, hgc->row_comm);
  ZOLTAN_FREE (&lpins[0]);                        /* we don't need lpins */
    
  /* recursively divide in two parts and repartition hypergraph */
  if (mid>lo) { /* only split if we really need it */
      if (!(left = (HGraph*) ZOLTAN_MALLOC (sizeof (HGraph))))
          MEMORY_ERROR;
      
      ierr = split_hypergraph (pins, hg, left, part, 0, zz);
      if (ierr != ZOLTAN_OK) 
          goto End;

      ierr = Zoltan_PHG_rdivide (lo, mid, final, zz, left, hgp, level+1);
      Zoltan_HG_HGraph_Free (left);
      if (ierr != ZOLTAN_OK) 
          goto End;
  } else {
      for (i = 0; i < hg->nVtx; ++i)
          if (part[i]==0)
              final [hg->vmap[i]] = lo;
  }

  if (hi>mid+1) { /* only split if we need it */
      if (!(right = (HGraph*) ZOLTAN_MALLOC (sizeof (HGraph))))
          MEMORY_ERROR;
      ierr = split_hypergraph (pins, hg, right, part, 1, zz);
  
      ZOLTAN_FREE (&pins[0]); /* we don't need pins */
      
      if (ierr != ZOLTAN_OK)
          goto End;

      ierr = Zoltan_PHG_rdivide (mid+1, hi, final, zz, right, hgp, level+1);
      Zoltan_HG_HGraph_Free (right);
  } else {
      ZOLTAN_FREE (&pins[0]); /* we don't need pins */
      for (i = 0; i < hg->nVtx; ++i)
          if (part[i]==1)
              final [hg->vmap[i]] = hi;
  }
      /* remove alloc'ed structs */
 End:
  Zoltan_Multifree (__FILE__, __LINE__, 5, &pins[0], &lpins[0], &part, &left, &right);

  return ierr;
}


static int split_hypergraph (int *pins[2], HGraph *ohg, HGraph *nhg, Partition part,
                             int partid, ZZ *zz)
{
  int *tmap = NULL;  /* temporary array mapping from old HGraph info to new */
  int edge, i, ierr=ZOLTAN_OK;  
  PHGComm *hgc = ohg->comm;
  char *yo = "split_hypergraph";

  Zoltan_HG_HGraph_Init (nhg);
  nhg->comm = ohg->comm;
  nhg->info               = ohg->info;
  nhg->VtxWeightDim       = ohg->VtxWeightDim;
  nhg->EdgeWeightDim      = ohg->EdgeWeightDim;
  
  /* allocate memory for dynamic arrays in new HGraph and for tmap array */
  if (ohg->nVtx && (tmap = (int*) ZOLTAN_MALLOC (ohg->nVtx * sizeof (int)))==NULL)
      MEMORY_ERROR;
  
  /* fill in tmap array, -1 for ignored vertices, otherwise nonnegative int */
  nhg->nVtx = 0;
  for (i = 0; i < ohg->nVtx; i++)
      tmap[i] = (part[i] == partid) ? nhg->nVtx++ : -1; 

  /* save vertex and edge weights if they exist */
  if (ohg->vwgt && nhg->VtxWeightDim)
    nhg->vwgt=(float*)ZOLTAN_MALLOC(nhg->nVtx*sizeof(float)*nhg->VtxWeightDim);
  if (nhg->nVtx && (nhg->vmap = (int*) ZOLTAN_MALLOC (nhg->nVtx * sizeof (int)))==NULL)
      MEMORY_ERROR;
  
  for (i = 0; i < ohg->nVtx; i++) {
      int v=tmap[i];
      if (v!=-1) {
          nhg->vmap[v] = ohg->vmap[i];
          if (nhg->vwgt)
              memcpy(&nhg->vwgt[v*nhg->VtxWeightDim], &ohg->vwgt[i*nhg->VtxWeightDim],
                     nhg->VtxWeightDim * sizeof(float));
      }
  }
    
  /* fill in hindex and hvertex arrays in new HGraph */
  nhg->nEdge = 0;
  nhg->nPins = 0;
  for (edge = 0; edge < ohg->nEdge; ++edge)
      if (pins[partid][edge] > 1) {
          ++nhg->nEdge;
          nhg->nPins += pins[partid][edge];
      }

  /* continue allocating memory for dynamic arrays in new HGraph */
  if (nhg->nEdge && (nhg->hindex  = (int*) ZOLTAN_MALLOC ((nhg->nEdge+1) * sizeof (int)))==NULL)
      MEMORY_ERROR;
  if (nhg->nPins && (nhg->hvertex = (int*) ZOLTAN_MALLOC (nhg->nPins * sizeof (int)))==NULL)
      MEMORY_ERROR;
  if (ohg->ewgt && nhg->EdgeWeightDim && nhg->nEdge)
      if ((nhg->ewgt=(float*)ZOLTAN_MALLOC(nhg->nEdge*sizeof(float)*nhg->EdgeWeightDim))==NULL)
          MEMORY_ERROR;
  
  nhg->nEdge = 0;
  nhg->nPins = 0;
  for (edge = 0; edge < ohg->nEdge; ++edge)
    if (pins[partid][edge] > 1) { /* edge has at least two vertices in partition:
                                        we are skipping size 1 nets */
      nhg->hindex[nhg->nEdge] = nhg->nPins;
      for (i = ohg->hindex[edge]; i < ohg->hindex[edge+1]; ++i)
        if (tmap [ohg->hvertex[i]] >= 0)  {
          nhg->hvertex[nhg->nPins] = tmap[ohg->hvertex[i]];
          nhg->nPins++;  
        }
        if (nhg->ewgt)
            memcpy(&nhg->ewgt[nhg->nEdge*nhg->VtxWeightDim], &ohg->vwgt[edge*nhg->VtxWeightDim],
                   nhg->EdgeWeightDim * sizeof(float));
        ++nhg->nEdge;
    }
  nhg->hindex[nhg->nEdge] = nhg->nPins;

  /* We need to compute dist_x, dist_y */
  if (!(nhg->dist_x = (int *) ZOLTAN_CALLOC((hgc->nProc_x+1), sizeof(int)))
	 || !(nhg->dist_y = (int *) ZOLTAN_CALLOC((hgc->nProc_y+1), sizeof(int))))
      MEMORY_ERROR;

  MPI_Scan(&nhg->nVtx, nhg->dist_x, 1, MPI_INT, MPI_SUM, hgc->row_comm);
  MPI_Allgather(nhg->dist_x, 1, MPI_INT, &(nhg->dist_x[1]), 1, MPI_INT, hgc->row_comm);
  nhg->dist_x[0] = 0;
  
  MPI_Scan(&nhg->nEdge, nhg->dist_y, 1, MPI_INT, MPI_SUM, hgc->col_comm);
  MPI_Allgather(nhg->dist_y, 1, MPI_INT, &(nhg->dist_y[1]), 1, MPI_INT, hgc->col_comm);
  nhg->dist_y[0] = 0;
    
  Zoltan_HG_Create_Mirror (zz, nhg);
 End:
  ZOLTAN_FREE (&tmap);
  return ierr;
}
