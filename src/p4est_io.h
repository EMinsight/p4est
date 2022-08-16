/*
  This file is part of p4est.
  p4est is a C library to manage a collection (a forest) of multiple
  connected adaptive quadtrees or octrees in parallel.

  Copyright (C) 2010 The University of Texas System
  Additional copyright (C) 2011 individual authors
  Written by Carsten Burstedde, Lucas C. Wilcox, and Tobin Isaac

  p4est is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  p4est is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with p4est; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/** \file p4est_io.h
 *
 * Provide functions to serialize/deserialize a forest.
 * Some are used as building blocks for \ref p4est_load and \ref p4est_save.
 * Others allow for saving and loading user-defined data to a parallel file.
 *
 * Furthermore, this module provides functions to write and read general data
 * files associated with a p4est.
 */

#ifndef P4EST_IO_H
#define P4EST_IO_H

#include <p4est.h>

SC_EXTERN_C_BEGIN;

#define P4EST_DATA_FILE_EXT "p4data" /**< file extension of p4est data files */
#define P4EST_MAGIC_NUMBER "p4data0" /**< magic string for p4est data files */
#define P4EST_NUM_METADATA_BYTES 64 /**< number of file metadata bytes */
#define P4EST_NUM_ARRAY_METADATA_BYTES 16 /**< number of array metadata bytes */
/* subtract 2 for '\n' at the beginning and end of the array metadata */
#define P4EST_NUM_ARRAY_METADATA_CHARS (P4EST_NUM_ARRAY_METADATA_BYTES - 2) /**< number of array metadata chars */
#define P4EST_BYTE_DIV 16 /**< All data blocks are padded to be divisible by this. */
#define P4EST_MAX_NUM_PAD_BYTES (P4EST_BYTE_DIV + 1) /**< We enforce to pad in any
                                                          case and the padding string
                                                          needs to contain two
                                                          newline characters and
                                                          therefore this is the
                                                          maximal number of pad
                                                          bytes. */
#define P4EST_NUM_USER_STRING_BYTES 48 /**< number of user string bytes */
#define P4EST_FILE_COUNT_ERROR -1 /**< All other error codes are defined by MPI or are
                                     errno. This error code is used to indicate a read
                                     or write count error that may be occured during a
                                     MPI IO operation or a IO operation called by C
                                     standard functions. */

/** Close an MPI file or its libsc-internal replacement in case of an error.
 * \param [in,out]  file    A sc_MPI_file
 * \return                  Always -1 since this function is only called
 *                          if an error already occured.
 */
int                 p4est_file_error_cleanup (sc_MPI_File * file);

/** This macro performs a clean up in the case of a MPI I/O open error.
 * We make use of the fact that sc_mpi_open is always called collectively.
 */
#define P4EST_FILE_CHECK_OPEN(errcode, fc, user_msg, cperrcode) do {\
                                            SC_CHECK_MPI_VERBOSE (errcode, user_msg);\
                                            *cperrcode = errcode;                    \
                                            if (errcode) {SC_FREE (fc->file);     \
                                            P4EST_FREE (fc);                         \
                                            return NULL;}} while (0)

/** The same as \ref P4EST_FILE_CHECK_OPEN but returns -1 instead of NULL */
#define P4EST_FILE_CHECK_INT(errcode, user_msg, cperrcode) do {\
                                            SC_CHECK_MPI_VERBOSE (errcode, user_msg);\
                                            *cperrcode = errcode;                    \
                                            if (errcode) {                           \
                                            return -1;}} while (0)

/** This macro prints the MPI error for sc_mpi_{read,write}_all and return NULL.
 * This means that this macro is appropriate to call it after a collective
 * read or write.
 */
#define P4EST_FILE_CHECK_NULL(errcode, fc, user_msg, cperrcode) do {\
                                            SC_CHECK_MPI_VERBOSE (errcode, user_msg);\
                                            *cperrcode = errcode;                    \
                                            if (errcode != sc_MPI_SUCCESS) {         \
                                            p4est_file_error_cleanup (&fc->file);    \
                                            P4EST_FREE (fc);                         \
                                            return NULL;}} while (0)

/* unused */
#if 0
/** The same as \ref P4EST_FILE_CHECK_NULL but returns void instead of NULL */
#define P4EST_FILE_CHECK_VOID(errcode, user_msg) do {SC_CHECK_MPI_VERBOSE (errcode, user_msg);\
                                            if (errcode != sc_MPI_SUCCESS) {                  \
                                            return;}} while (0)

/** The same as \ref P4EST_FILE_CHECK_VOID but closes the file */
/* TODO: Adjust close */
#define P4EST_FILE_CHECK_CLEAN_VOID(errcode, file, user_msg) do { int _mpiret;         \
                                            SC_CHECK_MPI_VERBOSE (errcode, user_msg);  \
                                            if (errcode != sc_MPI_SUCCESS) {           \
                                            _mpiret = MPI_File_close (&(file));        \
                                            SC_CHECK_MPI (_mpiret);                    \
                                            return;}} while (0)
#endif

/** This macro prints the MPI error for sc_mpi_{read,write}.
 * This means that this macro is appropriate to call it after a non-collective
 * read or write. For a correct error handling it is required to skip the rest
 * of the non-collective code and then broadcast the error flag.
 * Can be used only multiple times in a function but will always jump to the
 * same label. This leads to correct error managing.
 */
#define P4EST_FILE_CHECK_MPI(errcode, user_msg) do {SC_CHECK_MPI_VERBOSE (errcode, user_msg);\
                                                        if (mpiret != sc_MPI_SUCCESS) {\
                                                        goto p4est_read_write_error;}} while (0)

/** Use this macro after \ref P4EST_FILE_CHECK_MPI *directly* after the end of
 * non-collective statements.
 * Can be only used once in a function.
 */
/* Remark: Since we use a declaration after the label we need an empty statement. */
#define P4EST_HANDLE_MPI_ERROR(mpiret,fc,comm,cperrcode) do {p4est_read_write_error: ;\
                                                    int p4est_mpiret_handle_error =\
                                                    sc_MPI_Bcast (&mpiret, 1, sc_MPI_INT, 0, comm);\
                                                    SC_CHECK_MPI (p4est_mpiret_handle_error);\
                                                    *cperrcode = mpiret;\
                                                    if (mpiret) {\
                                                    p4est_file_error_cleanup (&fc->file);\
                                                    P4EST_FREE (fc);\
                                                    return NULL;}} while (0)

/** Extract processor local quadrants' x y level data.
 * Optionally extracts the quadrant data as well into a separate array.
 * \param [in] p4est    The forest is not modified.
 * \param [in,out] data If not NULL, pointer to a pointer that will be set
 *                      to a newly allocated array with per-quadrant data.
 *                      Must be NULL if p4est->data_size == 0.
 * \return              An array of type p4est_qcoord_t that contains
 *                      x y level for each quadrant on this processor.
 *                      The tree information is not extracted.
 */
sc_array_t         *p4est_deflate_quadrants (p4est_t * p4est,
                                             sc_array_t ** data);

/** Create a new p4est based on serialized data.
 * Its revision counter is set to zero.
 * See p4est.h and p4est_communication.h for more information on parameters.
 * \param [in] mpicomm       A valid MPI communicator.
 * \param [in] connectivity  This is the connectivity information that
 *                           the forest is built with.  Note that p4est
 *                           does not take ownership of the memory.
 * \param [in] global_first_quadrant First global quadrant on each proc and
 *                           one beyond.  Copied into global_first_quadrant.
 *                           Local count on rank is gfq[rank + 1] - gfq[rank].
 * \param [in] pertree       The cumulative quadrant counts per tree. 
 * \param [in] quadrants     Array as returned by p4est_deflate_quadrants.
 * \param [in] data          Array as from p4est_deflate_quadrants or NULL.
 *                           The elem_size of this array informs data_size.
 *                           Its elem_count equals the number of local quads.
 * \param [in] user_pointer  Assign to the user_pointer member of the p4est.
 * \return              The newly created p4est with a zero revision counter.
 */
p4est_t            *p4est_inflate (sc_MPI_Comm mpicomm,
                                   p4est_connectivity_t * connectivity,
                                   const p4est_gloidx_t *
                                   global_first_quadrant,
                                   const p4est_gloidx_t * pertree,
                                   sc_array_t * quadrants, sc_array_t * data,
                                   void *user_pointer);

/** p4est data file format
 * All p4est data files hava 64 bytes file metadata at the beginning of the file.
 * The metadata is written to the file as string without null-termination
 * (called string*) and is therefore readable in a text editor.
 *
 * File Metadata (64 bytes):
 * 7 bytes magic number (p4data0) and 1 byte new line char.
 * 23 bytes p4est version string* and 1 byte new line char.
 * 15 bytes number of global quadrants and 1 byte new line char.
 * 15 bytes user-header size in bytes and 1 byte new line char.
 *
 * After the file metadata the user can write a header of arbitrary
 * size (may be 0 bytes). The user-defined header is padded with spaces
 * such that number of bytes of the user-defined header is divisible by
 * 16.
 *
 * The actual data is stored in arrays corresponding to a mesh of a p4est.
 * This means that one data array stores a fixed number of bytes of user-
 * defined data per quadrant of a certain p4est. Therefore, one user-defined
 * data array is of the size p4est->global_num_quadrants * data_size, where
 * data_size is set by the user. The file format is partition independent.
 * The data arrays are padded by spaces such that the number of bytes for
 * an array is divisible by 16.
 * Every user data array is preceded by 64 bytes of array metadata written
 * by p4est. These 64 bytes are again written to the file as string* and can
 * be read using a text editor.
 *
 * Array Metadata (64 bytes):
 * 1 byte new line char, 14 bytes for the size in bytes of one array entry
 * and 1 byte new line char.
 * 47 bytes user-defined string* and 1 byte new line char.
 * 
 * The structure of p4est and p8est data files differs only by the magic number.
 *
 * The p4est metadata of a p4est data file can be accessed by \ref p4est_file_info().
 */

/** Opaque context used for writing a p4est data file. */
typedef struct p4est_file_context p4est_file_context_t;

/** Begin saving forest header and per-quadrant data into a parallel file.
 *
 * This function creates a new file or overwrites an existing one.
 * It is collective and creates the file on a parallel file system.
 * It takes an (optional) pointer to write a header of given size.
 * This function leaves the file open if MPI I/O is available.
 * It is necessary to call \ref
 * p4est_file_close (possibly after writing one or more data sets).
 * The file is opened in a write-only mode.
 *
 * We add some basic metadata to the file.
 * The file written contains the header data and data sets
 * as specified by the open/write functions called.
 * The header consists of the metadata header specified by p4est
 * followed by a user-defined header.
 *
 * It is the application's responsibility to write sufficient header
 * information to determine the number and size of the data sets
 * if such information is not recorded and maintained externally.
 * However, p4est makes some metadata accessible via
 * \ref p4est_file_info.
 *
 * This function does not abort on MPI I/O errors but returns NULL.
 *
 * \param [in] p4est          Valid forest.
 * \param [in] filename       Path to parallel file that is to be created.
 * \param [in] header_size    This number of bytes is written at the start
 *                            of the file on rank zero.  May be 0.
 * \param [in] header_data    A pointer to an array of header_size many
 *                            bytes. The data is written to the file as a
 *                            header.
 *                            For header_size == 0
 *                            the function does not write a user-header.
 *                            May be NULL if header_size == 0.
 *                            Must not be NULL on rank zero when
 *                            \a header_size is greater zero.
 * \param [out] errcode       An errcode that can be interpreted by \ref 
 *                            p4est_file_error_string and
 *                            \ref p4est_file_error_class.
 * \return                    Newly allocated context to continue writing
 *                            and eventually closing the file. NULL in
 *                            case of error.
 */
p4est_file_context_t *p4est_file_open_create
  (p4est_t * p4est, const char *filename,
   size_t header_size, const void *header_data, int *errcode);

/** Open a file for reading and read its header on rank zero.
 * The header data is broadcast to all ranks after reading.
 * The file must exist and be at least of the size of the header.
 * In practice, the header size should match the one writing the file.
 *
 * If the file has wrong metadata the function reports the error using
 * /ref P4EST_LERRORF, collectively close the file and deallocate
 * the file context. In this case the function returns NULL on all ranks.
 *
 * This function does not abort on MPI I/O errors but returns NULL.
 *
 * \param [in] p4est        The forest must be of the same refinement
 *                          pattern as the one used for writing the file.
 *                          Its global number of quadrants must match.
 *                          It is possible, however, to use a different
 *                          partition or number of ranks from writing it.
 * \param [in] filename     The path to the file that is opened.
 * \param [in] header_size  The size of the file header in number of bytes.
 *                          Can be determined by \ref p4est_file_info().
 * \param [out] header_data Already allocated data memory that will be filled
 *                          on all ranks with the file header.
 * \param [in,out] global_num_quadrants If NULL the number of global
 *                          quadrants in the file and in \a p4est must
 *                          coincide. Otherwise this condition does not
 *                          hold but the number of gloabl quadrants in
 *                          the file is written to \a global_num_quadrants
 *                          on all ranks. Therefore == NULL must hold
 *                          on all ranks or on none of them.
 * \param [out] errcode     An errcode that can be interpreted by \ref 
 *                          p4est_file_error_string and
 *                          \ref p4est_file_error_class.
 * \return                  Newly allocated context to continue reading
 *                          and eventually closing the file. NULL in
 *                          case of error.
 */
p4est_file_context_t *p4est_file_open_read (p4est_t * p4est,
                                            const char *filename,
                                            size_t header_size,
                                            void *header_data,
                                            p4est_gloidx_t *
                                            global_num_quadrants,
                                            int *errcode);

/** Write one (more) per-quadrant data set to a parallel output file.
 *
 * This function requires an opened file context.
 * The data set is appended to the header/previously written data sets.
 * This function writes a block of the size number of quadrants * data_size.
 *
 * This function does not abort on MPI I/O errors but returns NULL.
 *
 * \param [out] fc            Context previously created by \ref
 *                            p4est_file_open_create or \ref
 *                            p4est_file_open_append.
 * \param [in] quadrant_data  An array of the length number of local quadrants
 *                            with the element size equal to number of bytes
 *                            written per quadrant. The quadrant data is expected
 *                            to be stored according to the Morton order of
 *                            the quadrants. For quadrant_data->elem_size == 0
 *                            the function does nothing and returns the unchanged
 *                            file context.
 * \param [in] user_string    An array of maximal 47 bytes that is written
 *                            after the array-dependent metadata and before
 *                            the actual data. If the array is shorter the
 *                            written char array will be padded to the
 *                            right by spaces. The user_string is
 *                            written on rank 0 and therefore also only
 *                            required on rank 0. Can be NULL for other
 *                            ranks.
 * \param [out] errcode       An errcode that can be interpreted by \ref 
 *                            p4est_file_error_string and
 *                            \ref p4est_file_error_class.
 * \return                    Return the input context to continue writing
 *                            and eventually closing the file. The return value
 *                            is NULL if the function was called for
 *                            quadrant_data->elem_size == 0. The return
 *                            value is also NULL in case of error but then
 *                            it also holds errcode != 0 and the file is
 *                            tried to close and fc is freed.
 */
p4est_file_context_t *p4est_file_write_data (p4est_file_context_t * fc,
                                             sc_array_t * quadrant_data,
                                             char user_string[47],
                                             int *errcode);

/** Read one (more) per-quadrant data set from a parallel input file.
 * This function requires the appropriate number of readable bytes.
 * In practice, the data size to read should match the size written.
 * This function aborts if the number of bytes to read is bigger than the
 * datatset that corresponds to the processor.
 * The data size to read is encoded by the element size of quadrant_data
 * It is possible to skip over a data set to read by a NULL \ref sc_array.
 * It is legal to close a file before all data sets have been read.
 *
 * The function closes and deallocates the file context and returns NULL
 * if the bytes the user wants to read exceed the given file and/or
 * the element size of the array given by quadrant_data->elem_size does not
 * coincide with the element size according to the array metadata given in
 * the file.
 *
 * This function does not abort on MPI I/O errors but returns NULL.
 *
 * \param [in,out] fc         Context previously created by \ref
 *                            p4est_file_open_read.  It keeps track
 *                            of the data sets read one after another.
 * \param [in,out] quadrant_data  An array of the length number of local quadrants
 *                            with the element size equal to number of bytes
 *                            read per quadrant. The quadrant data is read
 *                            according to the Morton order of the quadrants.
 *                            For quadrant_data->elem_size == 0
 *                            the function does nothing and returns the unchanged
 *                            file context. For quadrant_data == NULL the
 *                            function skips one data array in the file.
 * \param [in,out]  user_string On rank 0 at least 48 bytes. Can be NULL
 *                             for other ranks since it is only filled
 *                             for rank 0. Can be also NULL if
 *                             quadrant_data is NULL.
 * \param [out] errcode       An errcode that can be interpreted by \ref 
 *                            p4est_file_error_string and
 *                            \ref p4est_file_error_class.
 * \return                    Return a pointer to input context or NULL in case
 *                            of errors that does not abort the program or if
 *                            the function was called with quadrant_data == NULL.
 *                            In case of error the file is tried to close
 *                            and fc is freed.
 */
p4est_file_context_t *p4est_file_read_data (p4est_file_context_t * fc,
                                            sc_array_t * quadrant_data,
                                            char *user_string, int *errcode);

/** Read metadata information of a file written by a matching forest.
 * Matching refers to the global count of quadrants; partition is irrelevant.
 *
 * This function parses the given file on rank 0 and broadcasts the information
 * on the number of data fields contained to all other ranks.  Collective call.
 *
 * This function catches all I/O and file format errors and returns a valid MPI
 * error class related to file handling.  Errors are collectively synchronized.
 *
 * If the number of bytes that the user intend to read is larger than the number
 * bytes left in the file, the function prints out an information about this
 * situation using \ref P4EST_LERROR. In this case the function reads the bytes
 * that are possible to read but returns NULL to indicate an error. The file
 * context fc can be used again to read the same field with different element
 * count and size since the fc metadata about the current array stays unchanged.
 *
 * \param [in]  p4est               A p4est that is only required for the
 *                                  MPI communicator, and to verify the
 *                                  global quadrant count found in the file.
 * \param [in]  filename            Path to parallel file.
 * \param [out] header_size         The size of the user-defined header in bytes.
 * \param [in,out] data_sizes       After a successful function call this
 *                                  variable holds an array with a length
 *                                  corresponding to the number of arrays in the
 *                                  file that are successfully found and seeked.
 *                                  The values in the array are the
 *                                  number of bytes of stored data per quadrant.
 *                                  Require elem_size->elem_size == sizeof (size_t)
 *                                  on input and preserve it on output.
 * \param [out] errcode             An errcode that can be interpreted by \ref 
 *                                  p4est_file_error_string and
 *                                  \ref p4est_file_error_class.
 * \return                          0 for a successful call and -1 in case of
 *                                  an error. See also \ref errcode argument.
 */
int                 p4est_file_info (p4est_t * p4est, const char *filename,
                                     size_t * header_size,
                                     sc_array_t * data_sizes, int *errcode);

/** Converts a p4est file error code into a p4est_file error class.
 * This function turns MPI error codes into MPI error classes if
 * MPI IO is enabled.
 * If MPI IO is not enabled, the function processes the errors outside
 * of MPI but passes version 1.1 errors to MPI_Error_class.
 * Furthermore, p4est_file functions can create \ref P4EST_FILE_COUNT_ERROR
 * as errcode what is also processed by this function.
 * \param [in]  errcode     An errcode from a p4est_file function.
 * \param [out] errclass    Non-NULL pointer. Filled with matching
 *                          errclass on success.
 * \return                  sc_MPI_SUCCESS on successful conversion.
 *                          Other MPI error code otherwise.
 */
int                 p4est_file_error_class (int errcode, int *errclass);

/** Turn p4est_file errcode into a string.
 * errclass must be a class that is output by \ref p4est_file_error_class.
 * \param [in] errclass     An errclass that is output by \ref
 *                          p4est_file_error_class.
 * \param [in,out] string   At least sc_MPI_MAX_ERROR_STRING bytes.
 * \param [out] resultlen   Length of string on return.
 * \return                   sc_MPI_SUCCESS on success or
 *                           something else on invalid arguments.
 */
int                 p4est_file_error_string (int errclass, char *string,
                                             int *resultlen);

/** Close a file opened for parallel write/read and free the context.
 * \param [in,out] fc       Context previously created by \ref
 *                          p4est_file_open_create, \ref
 *                          p4est_file_open_append, or \ref
 *                          p4est_file_open_read.  Is freed.
 * \param [out] errcode     An errcode that can be interpreted by \ref 
 *                          p4est_file_error_string and
 *                          \ref p4est_file_error_class.
 * \return                  0 for a successful call and -1 in case of
 *                          an error. See also \ref errcode argument.
 */
int                 p4est_file_close (p4est_file_context_t * fc,
                                      int *errcode);

/** A macro to check for file write related count errors.
 * These errors are handeled as fatal errors. The macro is only applicable for
 * collective calls.
 */
#define P4EST_FILE_CHECK_COUNT(icount,ocount,fc,cperrcode) do { int p4est_count_error_global, p4est_mpiret;\
                                                 int p4est_file_check_count = ((int) icount != ocount);    \
                                                 p4est_mpiret = sc_MPI_Allreduce (&p4est_file_check_count, \
                                                 &p4est_count_error_global, 1, sc_MPI_INT, sc_MPI_LOR,     \
                                                 fc->p4est->mpicomm);                                      \
                                                 SC_CHECK_MPI (p4est_mpiret);                              \
                                                 *cperrcode = (p4est_file_check_count) ?                   \
                                                 P4EST_FILE_COUNT_ERROR : sc_MPI_SUCCESS;                  \
                                                 if (p4est_count_error_global)                             \
                                                 { if (fc->p4est->mpirank == 0) {                          \
                                                  SC_LERRORF ("Count error at %s:%d.\n",__FILE__,          \
                                                 __LINE__);}                                               \
                                                 p4est_file_error_cleanup (&fc->file);      \
                                                 P4EST_FREE (fc);                                 \
                                                 return NULL;}} while (0)

/** A macro to check for file write related count errors. This macro is
 * only applicable for serial calls. The errors are handeled as fatal errors.
 * We assume that the macro is called on rank 0.
 */
#define P4EST_FILE_CHECK_COUNT_SERIAL(icount, ocount) do {if (((int) icount) != ocount) {\
                                                        SC_LERRORF ("Count error on rank 0 at %s:%d.\n",__FILE__,\
                                                        __LINE__);\
                                                        goto p4est_write_count_error;}} while (0)

/** A macro to handle a file write error that occured on rank 0 but need to be
 * handeled collectivly. We need count_error as input since we need a variable to
 * broadcast the count error status. count_error is true if there is a count error
 * and false otherwise.
 */
/* Remark: Since we use a declaration after the label we need an empty statement. */
#define P4EST_HANDLE_MPI_COUNT_ERROR(count_error,fc,cperrcode) do {p4est_write_count_error: ;\
                                                    int p4est_mpiret_handle = sc_MPI_Bcast (&count_error, 1, sc_MPI_INT, 0,\
                                                    fc->p4est->mpicomm);\
                                                    SC_CHECK_MPI (p4est_mpiret_handle);\
                                                    *cperrcode = (count_error) ? P4EST_FILE_COUNT_ERROR : sc_MPI_SUCCESS;\
                                                    if (count_error) {\
                                                    p4est_file_error_cleanup (&fc->file);\
                                                    P4EST_FREE (fc);\
                                                    return NULL;}} while (0)

SC_EXTERN_C_END;

#endif /* !P4EST_IO_H */
