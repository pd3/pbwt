/*  File: pbwtPaint.c
 *  Author: Richard Durbin (rd@sanger.ac.uk)
 *  Copyright (C) Genome Research Limited, 2013-
 *-------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------
 * Description: tools for chromosome painting as in ChromoPainter, FineStructure etc.
 * Exported functions:
 * HISTORY:
 * Last edited: Jul 18 14:24 2014 (rd)
 * Created: Tue Apr  1 11:34:41 2014 (rd)
 *-------------------------------------------------------------------
 */

#include "pbwt.h"

#define IDEA3

double **counts = 0 ;
double **counts2 = 0 ; /* store sums of squares of counts in 100 block bins */

#ifdef IDEA2
static void reportMatch (int i, int j, int start, int end) 
{ counts[i][j] += (end - start) ; }
#endif
#ifdef IDEA3
typedef struct { int j ; int start ; int end ; } MatchSegment ;
/* matches are semi-open [start,end) so length is end-start */
static Array *maxMatch = 0 ;	/* arrays of MatchSegment */
static void reportMatch (int i, int j, int start, int end)
{ MatchSegment *ms = arrayp (maxMatch[i], arrayMax(maxMatch[i]), MatchSegment) ;
  ms->j = j ; ms->start = start ; ms->end = end ;
}
#endif

void paintAncestryMatrix (PBWT *p, char* fileRoot)
{
  int i, j, k ;
  counts = myalloc (p->M, double*) ; counts2 = myalloc (p->M, double*) ;
  for (i = 0 ; i < p->M ; ++i) 
    { counts[i] = mycalloc (p->M, double) ;
      counts2[i] = mycalloc (p->M, double) ;
    }

#ifdef IDEA1	/* original idea written with Dan Lawson Newton Institute 140402 */
  PbwtCursor *u = pbwtCursorCreate (p, TRUE, TRUE) ;
  int k ;
  for (k = 0 ; k < p->N ; k++)
    { for (j = 1 ; j < p->M ; ++j)
	if (u->y[j] != u->y[j-1])
	  { if (j < p->M || u->d[j] < u->d[j+1]) ++counts[u->a[j]][u->a[j-1]] ;
	    if (j > 1 || u->d[j] < u->d[j-1]) ++counts[u->a[j-1]][u->a[j]] ;
	  }
      pbwtCursorForwardsReadAD (u, k) ;
    }
#endif
#ifdef IDEA2  /* count maximal matches */
  matchMaximalWithin (p, reportMatch) ;
#endif
#ifdef IDEA3  /* weight maximal matches per site as when imputing */
  maxMatch = myalloc (p->M, Array) ;
  for (i = 0 ; i < p->M ; ++i) maxMatch[i] = arrayCreate (1024, MatchSegment) ;
  matchMaximalWithin (p, reportMatch) ;  /* store maximal matches in maxMatch */
  double *partCounts = myalloc (p->M, double) ;
  /* now weight per site based on distance from ends */
  for (i = 0 ; i < p->M ; ++i)
    { MatchSegment *m1 = arrp(maxMatch[i],0,MatchSegment), *m ;
      int n1 = 1 ;		/* so don't have an empty chunk to start with! */
      MatchSegment *mStop = arrp(maxMatch[i], arrayMax(maxMatch[i])-1, MatchSegment) ;
      memset (partCounts, 0, sizeof(double)*p->M) ;
      for (k = 1 ; k < p->N ; k++)
	{ double sum = 0 ;
	  while (m1->end <= k && m1 < mStop)
	    { if (!(n1 % 100))
		{ int jj ; for (jj = 0 ; jj < p->M ; ++jj) 
			     counts2[i][jj] += partCounts[jj]*partCounts[jj] ;
		  memset (partCounts, 0, sizeof(double)*p->M) ;
		  /* we should deal with the final counts2 block */
		}
	      ++m1 ; ++n1 ;
	    }
	  for (m = m1 ; m->start < k && m <= mStop ; ++m) 
	    sum += (k - m->start) * (m->end - k) ;
	  if (sum)
	    for (m = m1 ; m->start < k && m <= mStop ; ++m) 
	      counts[i][m->j] += (k - m->start) * (m->end - k) / sum ;
	}
    }
  free (partCounts) ;
#endif

  /* report results */
  double *totCounts = mycalloc (p->M, double) ;
  FILE *fc = fopenTag (fileRoot, "counts", "w") ;
  FILE *fc2 = fopenTag (fileRoot, "counts2", "w") ;
  for (i = 0 ; i < p->M ; ++i)
    { for (j = 0 ; j < p->M ; ++j) 
	{ fprintf (fc, " %8.4g", counts[i][j]) ; 
	  totCounts[i] += counts[i][j] ; 
	}
      fputc ('\n', fc) ;
      for (j = 0 ; j < p->M ; ++j) fprintf (fc2," %8.4g", counts2[i][j]) ; 
      fputc ('\n', fc2) ;
      if (isCheck && (i%2) && p->samples) 
	fprintf (stderr, "%s %8.4g %8.4g\n", 
		 sampleName (sample(p,i-1)), totCounts[i-1], totCounts[i]) ;
    }
  fclose (fc) ; fclose (fc2) ;

#define HORRIBLE_HACK
#ifdef HORRIBLE_HACK
  int i1, j1 ;
  for (i = 0 ; i < 5 ; ++i)
    { for (j = 0 ; j < 5 ; ++j)
	{ double sum = 0 ;
	  int kPop = p->M/5 ;
	  for (i1 = kPop*i ; i1 < kPop*(i+1) ; ++i1)
	    for (j1 = kPop*j ; j1 < kPop*(j+1) ; ++j1)
	      sum += counts[i1][j1] ;
	  if (j != i) fprintf (stderr, " %8.4g", sum / (kPop*kPop)) ;
	  else fprintf (stderr, " %8.4g", sum / (kPop*kPop)) ;
	}
      fputc ('\n', stderr) ;
    }
 #endif

  /* clean up */
  for (i = 0 ; i < p->M ; ++i) { free (counts[i]) ; free (counts2[i]) ; }
  free (counts) ; free (counts2) ; free (totCounts) ;
}

/* end of file */
