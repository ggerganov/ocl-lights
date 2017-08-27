/*****************************************************************************
 *
 * File Name : utils.h
 *
 * Creation Date : 30-11-2015
 *
 * Last Modified : Tue Dec  1 14:42:34 2015
 *
 *****************************************************************************/

/*! \file utils.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#define GET_WORK_DOMAIN(nTotal) \
\
const uint lid = get_local_id(0); \
const uint gid = get_group_id(0); \
\
const uint lsize = get_local_size(0); \
const uint ngrps = get_num_groups(0); \
\
uint nPerGroup = ((nTotal) + ngrps - 1)/ngrps;\
\
uint id = mad24(gid, nPerGroup, lid); \
const uint idmax = min(mul24((gid+1), nPerGroup), (uint)(nTotal));

#define GET_XY(id, size2, ix, iy) \
uint ix = id; \
uint iy = ix/size2.x; \
ix -= mul24(iy, (uint)(size2.x));
