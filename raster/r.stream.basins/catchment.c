#include <grass/glocale.h>
#include "global.h"

/* 
   Link: a channel between junction
   Ooutlet: is final cell of every segment
   Segment: a channel which order remains unchanged in spite it pass through junction or not
   Number of outlets shall be equal the number of segments
   Number of junction shall be less than number of links
 */

/*
   find outlets create table of outlets point, with r, c and value. depending of flag:
   if flag -l is as anly last points of segment is added to table and uses the value is the category as value for whole basins
   if flag -l = flase (default) last points of every link is added to table and nd uses the value is the category as value for subbasin
   In both cases if flag -c is used it add out_num+1 value as value of point. That structure is next used in reset_catchments and fill_catchments fuctions
 */
int find_outlets(void)
{
    int d, i, j;		/* d: direction, i: iteration */
    int r, c;
    int next_stream = -1, cur_stream;
    int out_max = ncols + nrows;

    int nextr[9] = { 0, -1, -1, -1, 0, 1, 1, 1, 0 };
    int nextc[9] = { 0, 1, 0, -1, -1, -1, 0, 1, 1 };

    G_message(_("Finding nodes..."));
    outlets = (OUTLET *) G_malloc((out_max) * sizeof(OUTLET));

    outlets_num = 0;

    for (r = 0; r < nrows; ++r) {
	for (c = 0; c < ncols; ++c) {
	    if (streams[r][c] > 0) {
		if (outlets_num > (out_max - 1)) {
		    outlets =
			(OUTLET *) G_realloc(outlets,
					     out_max * 2 * sizeof(OUTLET));
		}

		d = abs(dirs[r][c]);	/* abs */
		if (r + nextr[d] < 0 || r + nextr[d] > (nrows - 1) ||
		    c + nextc[d] < 0 || c + nextc[d] > (ncols - 1)) {
		    next_stream = -1;	/* border */
		}
		else {
		    next_stream = streams[r + nextr[d]][c + nextc[d]];
		    if (next_stream < 1)
			next_stream = -1;
		}
		if (d == 0)
		    next_stream = -1;
		cur_stream = streams[r][c];

		if (lasts) {
		    if (cur_stream != next_stream && next_stream < 0) {	/* is outlet! */
			outlets[outlets_num].r = r;
			outlets[outlets_num].c = c;
			outlets[outlets_num].val =
			    (cats) ? outlets_num + 1 : streams[r][c];
			outlets_num++;
		    }
		}
		else {
		    if (cur_stream != next_stream) {	/* is outlet or node! */
			outlets[outlets_num].r = r;
			outlets[outlets_num].c = c;
			outlets[outlets_num].val =
			    (cats) ? outlets_num + 1 : streams[r][c];
			outlets_num++;
		    }
		}		/* end lasts */
	    }			/* end if streams */
	}			/* end for */
    }				/* end for */

    return 0;
}

/*
   this function reset all values to 0 and adds points from outlet structure 
   The lines below from function fill_catchments makes that autlet is not added do fifo
   queue and basin is limited by next outlet

   if (catchments[r + nextr[i]][c + nextc[i]]>0)
   continue; 

   It is simple trick but allow gives module its real funcionality
 */
int reset_catchments(void)
{
    int r, c, i;

    for (r = 0; r < nrows; ++r) {
	for (c = 0; c < ncols; ++c) {
	    streams[r][c] = 0;
	}
    }
    for (i = 0; i < outlets_num; ++i) {
	streams[outlets[i].r][outlets[i].c] = outlets[i].val;
    }
    return 0;
}

/*
   algorithm uses fifo queue for determining basins area. 
 */
int fill_catchments(OUTLET outlet)
{

    int nextr[9] = { 0, -1, -1, -1, 0, 1, 1, 1, 0 };
    int nextc[9] = { 0, 1, 0, -1, -1, -1, 0, 1, 1 };

    int r, c, val, i, j;
    POINT n_cell;

    tail = 0;
    head = -1;
    r = outlet.r;
    c = outlet.c;
    val = outlet.val;

    streams[r][c] = val;

    while (tail != head) {
	for (i = 1; i < 9; ++i) {
	    if (r + nextr[i] < 0 || r + nextr[i] > (nrows - 1) ||
		c + nextc[i] < 0 || c + nextc[i] > (ncols - 1))
		continue;	/* border */
	    j = (i + 4) > 8 ? i - 4 : i + 4;
	    if (dirs[r + nextr[i]][c + nextc[i]] == j) {	/* countributing cell */

		if (streams[r + nextr[i]][c + nextc[i]] > 0)
		    continue;	/* other outlet */

		streams[r + nextr[i]][c + nextc[i]] = val;
		n_cell.r = (r + nextr[i]);
		n_cell.c = (c + nextc[i]);
		fifo_insert(n_cell);
	    }
	}			/* end for i... */

	n_cell = fifo_return_del();
	r = n_cell.r;
	c = n_cell.c;
    }

    return 0;
}

/* fifo functions */
int fifo_insert(POINT point)
{
    fifo_outlet[tail++] = point;
    if (tail > fifo_max)
	tail = 0;
    return 0;
}

POINT fifo_return_del(void)
{
    if (head > fifo_max)
	head = -1;
    return fifo_outlet[++head];
}
