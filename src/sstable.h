minor_compaction
major_compaction

//--------
// file

typedef struct {
} SSFTBL;

SSFTBL *ssftblnew();
void ssftbldel(SSFTBL *tbl);

int ssftblopen(SSFTBL *tbl, const char *path, int omode);
int ssftblclose(SSFTBL *tbl);

int ssftblput(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
int ssftblget(SSFTBL *tbl, const void *kbuf, int ksiz, int *sp);

//--------
// memory

typedef struct {
} SSMTBL;

SSMTBL *ssmtblnew();
void ssmtbldel(SSMTBL *tbl);

int ssmtblopen(SSMTBL *tbl, const char *path, int omode);
int ssmtblclose(SSMTBL *tbl);

int ssmtblput(SSMTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
int ssmtblget(SSMTBL *tbl, const void *kbuf, int ksiz, int *sp);
