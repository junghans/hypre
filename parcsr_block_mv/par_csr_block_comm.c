/*BHEADER**********************************************************************
 * Copyright (c) 2006   The Regents of the University of California.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by the HYPRE team. UCRL-CODE-222953.
 * All rights reserved.
 *
 * This file is part of HYPRE (see http://www.llnl.gov/CASC/hypre/).
 * Please see the COPYRIGHT_and_LICENSE file for the copyright notice, 
 * disclaimer, contact information and the GNU Lesser General Public License.
 *
 * HYPRE is free software; you can redistribute it and/or modify it under the 
 * terms of the GNU General Public License (as published by the Free Software
 * Foundation) version 2.1 dated February 1999.
 *
 * HYPRE is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS 
 * FOR A PARTICULAR PURPOSE.  See the terms and conditions of the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * $Revision$
 ***********************************************************************EHEADER*/




#include "headers.h"                                                                                                           
#include "_hypre_utilities.h"
#include "_hypre_parcsr_mv.h"

/*--------------------------------------------------------------------------
 * hypre_ParCSRBlockCommHandleCreate
 *--------------------------------------------------------------------------*/

hypre_ParCSRCommHandle *
hypre_ParCSRBlockCommHandleCreate(int job, int bnnz, hypre_ParCSRCommPkg *comm_pkg,
                                  void *send_data, void *recv_data )
{
   int      num_sends = hypre_ParCSRCommPkgNumSends(comm_pkg);
   int      num_recvs = hypre_ParCSRCommPkgNumRecvs(comm_pkg);
   MPI_Comm comm      = hypre_ParCSRCommPkgComm(comm_pkg);
   hypre_ParCSRCommHandle *comm_handle;
   int         num_requests;
   MPI_Request *requests;
   int    i, j, my_id, num_procs, ip, vec_start, vec_len;
   double *d_send_data = (double *) send_data;
   double *d_recv_data = (double *) recv_data;
                  
/*---------------------------------------------------------------------------
    * job = 1 : is used to initialize communication exchange for the parts
    *		of vector needed to perform a Matvec,  it requires send_data 
    *		and recv_data to be doubles, recv_vec_starts and 
    *		send_map_starts need to be set in comm_pkg.
    * job = 2 : is used to initialize communication exchange for the parts
    *		of vector needed to perform a MatvecT,  it requires send_data 
    *		and recv_data to be doubles, recv_vec_starts and 
    *		send_map_starts need to be set in comm_pkg.
    *------------------------------------------------------------------------*/


   num_requests = num_sends + num_recvs;
   requests = hypre_CTAlloc(MPI_Request, num_requests);
 
   MPI_Comm_size(comm,&num_procs);
   MPI_Comm_rank(comm,&my_id);

   j = 0;

   switch (job)
   {
      case  1:
      {
         for (i = 0; i < num_recvs; i++)
         {
            ip = hypre_ParCSRCommPkgRecvProc(comm_pkg, i); 
            vec_start = hypre_ParCSRCommPkgRecvVecStart(comm_pkg,i);
            vec_len = (hypre_ParCSRCommPkgRecvVecStart(comm_pkg,i+1)-vec_start)*bnnz;
            MPI_Irecv(&d_recv_data[vec_start*bnnz], vec_len, MPI_DOUBLE,
                      ip, 0, comm, &requests[j++]);
         }
         for (i = 0; i < num_sends; i++)
         {
            vec_start = hypre_ParCSRCommPkgSendMapStart(comm_pkg, i);
            vec_len = (hypre_ParCSRCommPkgSendMapStart(comm_pkg,i+1)-vec_start)*bnnz;
            ip = hypre_ParCSRCommPkgSendProc(comm_pkg, i); 
            MPI_Isend(&d_send_data[vec_start*bnnz], vec_len, MPI_DOUBLE, ip, 0, comm, 
                      &requests[j++]);
         }
         break;
      }
      case  2:
      {

         for (i = 0; i < num_sends; i++)
         {
	    vec_start = hypre_ParCSRCommPkgSendMapStart(comm_pkg, i);
	    vec_len = (hypre_ParCSRCommPkgSendMapStart(comm_pkg, i+1) - vec_start)*bnnz;
      	    ip = hypre_ParCSRCommPkgSendProc(comm_pkg, i); 
   	    MPI_Irecv(&d_recv_data[vec_start*bnnz], vec_len, MPI_DOUBLE,
                      ip, 0, comm, &requests[j++]);
         }
         for (i = 0; i < num_recvs; i++)
         {
            ip = hypre_ParCSRCommPkgRecvProc(comm_pkg, i); 
            vec_start = hypre_ParCSRCommPkgRecvVecStart(comm_pkg,i);
            vec_len = (hypre_ParCSRCommPkgRecvVecStart(comm_pkg,i+1)-vec_start)*bnnz;
            MPI_Isend(&d_send_data[vec_start*bnnz], vec_len, MPI_DOUBLE,
                      ip, 0, comm, &requests[j++]);
         }
         break;
      }
   }
   

  /*--------------------------------------------------------------------
   * set up comm_handle and return
   *--------------------------------------------------------------------*/

   comm_handle = hypre_CTAlloc(hypre_ParCSRCommHandle, 1);

   hypre_ParCSRCommHandleCommPkg(comm_handle)     = comm_pkg;
   hypre_ParCSRCommHandleSendData(comm_handle)    = send_data;
   hypre_ParCSRCommHandleRecvData(comm_handle)    = recv_data;
   hypre_ParCSRCommHandleNumRequests(comm_handle) = num_requests;
   hypre_ParCSRCommHandleRequests(comm_handle)    = requests;
   return ( comm_handle );
}




/*--------------------------------------------------------------------
hypre_ParCSRBlockCommHandleDestroy
 *--------------------------------------------------------------------*/

int
hypre_ParCSRBlockCommHandleDestroy(hypre_ParCSRCommHandle *comm_handle)
{
   MPI_Status          *status0;

   if ( comm_handle==NULL ) return hypre_error_flag;

   if (hypre_ParCSRCommHandleNumRequests(comm_handle))
   {
      status0 = hypre_CTAlloc(MPI_Status,
                       hypre_ParCSRCommHandleNumRequests(comm_handle));
      MPI_Waitall(hypre_ParCSRCommHandleNumRequests(comm_handle),
                  hypre_ParCSRCommHandleRequests(comm_handle), status0);
      hypre_TFree(status0);
   }

   hypre_TFree(hypre_ParCSRCommHandleRequests(comm_handle));
   hypre_TFree(comm_handle);

   return hypre_error_flag;
}




/*--------------------------------------------------------------------
 * hypre_ParCSRBlockMatrixCreateAssumedPartition -
 * Each proc gets it own range. Then 
 * each needs to reconcile its actual range with its assumed
 * range - the result is essentila a partition of its assumed range -
 * this is the assumed partition.   
 *--------------------------------------------------------------------*/


int
hypre_ParCSRBlockMatrixCreateAssumedPartition( hypre_ParCSRBlockMatrix *matrix) 
{


   int global_num_cols;
   int myid;
   int  row_start=0, row_end=0, col_start = 0, col_end = 0;

   MPI_Comm   comm;
   
   hypre_IJAssumedPart *apart;

   global_num_cols = hypre_ParCSRBlockMatrixGlobalNumCols(matrix); 
   comm = hypre_ParCSRBlockMatrixComm(matrix);
   
   /* find out my actualy range of rows and columns */
   row_start = hypre_ParCSRBlockMatrixFirstRowIndex(matrix);
   row_end = hypre_ParCSRBlockMatrixLastRowIndex(matrix);
   col_start =  hypre_ParCSRBlockMatrixFirstColDiag(matrix);
   col_end =  hypre_ParCSRBlockMatrixLastColDiag(matrix);

   MPI_Comm_rank(comm, &myid );

   /* allocate space */
   apart = hypre_CTAlloc(hypre_IJAssumedPart, 1);

  /* get my assumed partitioning  - we want partitioning of the vector that the
      matrix multiplies - so we use the col start and end */
   hypre_GetAssumedPartitionRowRange( myid, global_num_cols, &(apart->row_start), 
                                             &(apart->row_end));

  /*allocate some space for the partition of the assumed partition */
    apart->length = 0;
    /*room for 10 owners of the assumed partition*/ 
    apart->storage_length = 10; /*need to be >=1 */ 
    apart->proc_list = hypre_TAlloc(int, apart->storage_length);
    apart->row_start_list =   hypre_TAlloc(int, apart->storage_length);
    apart->row_end_list =   hypre_TAlloc(int, apart->storage_length);


    /* now we want to reconcile our actual partition with the assumed partition */
    hypre_LocateAssummedPartition(col_start, col_end, global_num_cols, apart, myid);

    /* this partition will be saved in the matrix data structure until the matrix is destroyed */
    hypre_ParCSRBlockMatrixAssumedPartition(matrix) = apart;
   
    return hypre_error_flag;
    
}

/*--------------------------------------------------------------------
 * hypre_ParCSRMatrixDestroyAssumedPartition
 *--------------------------------------------------------------------*/
int 
hypre_ParCSRBlockMatrixDestroyAssumedPartition(hypre_ParCSRBlockMatrix *matrix )
{

   hypre_IJAssumedPart *apart;
   
   apart = hypre_ParCSRMatrixAssumedPartition(matrix);
   

   if(apart->storage_length > 0) 
   {      
      hypre_TFree(apart->proc_list);
      hypre_TFree(apart->row_start_list);
      hypre_TFree(apart->row_end_list);
      hypre_TFree(apart->sort_index);
   }

   hypre_TFree(apart);
   

   return (0);
}
