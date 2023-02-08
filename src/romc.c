/**************************************************************
        LZSS.C -- A Data Compression Program
        (tab = 4 spaces)
***************************************************************
        4/6/1989 Haruhiko Okumura
        Use, distribute, and modify this program freely.
        Please send me your improved versions.
                PC-VAN		SCIENCE
                NIFTY-Serve	PAF01022
                CompuServe	74050,1022
**************************************************************/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
typedef unsigned long u32;

#define N 4096 /* size of ring buffer */
#define F 18   /* upper limit for match_length */
#define THRESHOLD                                       \
    2         /* encode string into position and length \
                                     if match_length is greater than this */
#define NIL N /* index for root of binary search trees */

unsigned long int fourmbit = 4194304,       /* 4mbit rom size */
    textsize = 0,                           /* text size counter */
    codesize = 0;                           /* code size counter */
uint8_t text_buf[N + F - 1];          /* ring buffer of size N,
                            with extra F-1 bytes to facilitate string comparison */
int match_position, match_length,           /* of longest match.  These are
                     set by the InsertNode() procedure. */
    lson[N + 1], rson[N + 257], dad[N + 1]; /* left & right children &
           parents -- These constitute binary search trees. */

void init_tree(void) /* initialize trees */
{
    /* For i = 0 to N - 1, rson[i] and lson[i] will be the right and
       left children of node i.  These nodes need not be initialized.
       Also, dad[i] is the parent of node i.  These are initialized to
       NIL (= N), which stands for 'not used.'
       For i = 0 to 255, rson[N + i + 1] is the root of the tree
       for strings that begin with character i.  These are initialized
       to NIL.  Note there are 256 trees. */

    for (int i = N + 1; i <= N + 256; i++)
        rson[i] = NIL;
    for (int i = 0; i < N; i++)
        dad[i] = NIL;
}

void insert_node(int r)
/* Inserts string of length F, text_buf[r..r+F-1], into one of the
   trees (text_buf[r]'th tree) and returns the longest-match position
   and length via the global variables match_position and match_length.
   If match_length = F, then removes the old node in favor of the new
   one, because the old one will be deleted sooner.
   Note r plays double role, as tree node and position in buffer. */
{
    int i, p, cmp;
    uint8_t *key;

    cmp = 1;
    key = &text_buf[r];
    p = N + 1 + key[0];
    rson[r] = lson[r] = NIL;
    match_length = 0;
    while (1) {
        if (cmp >= 0) {
            if (rson[p] != NIL)
                p = rson[p];
            else {
                rson[p] = r;
                dad[r] = p;
                return;
            }
        } else {
            if (lson[p] != NIL)
                p = lson[p];
            else {
                lson[p] = r;
                dad[r] = p;
                return;
            }
        }
        for (i = 1; i < F; i++)
            if ((cmp = key[i] - text_buf[p + i]) != 0)
                break;
        if (i > match_length) {
            match_position = p;
            if ((match_length = i) >= F)
                break;
        }
    }
    dad[r] = dad[p];
    lson[r] = lson[p];
    rson[r] = rson[p];
    dad[lson[p]] = r;
    dad[rson[p]] = r;
    if (rson[dad[p]] == p)
        rson[dad[p]] = r;
    else
        lson[dad[p]] = r;
    dad[p] = NIL; /* remove p */
}

void delete_node(int p) /* deletes node p from tree */
{
    int q;

    if (dad[p] == NIL)
        return; /* not in tree */
    if (rson[p] == NIL)
        q = lson[p];
    else if (lson[p] == NIL)
        q = rson[p];
    else {
        q = lson[p];
        if (rson[q] != NIL) {
            do {
                q = rson[q];
            } while (rson[q] != NIL);
            rson[dad[q]] = lson[q];
            dad[lson[q]] = dad[q];
            lson[q] = lson[p];
            dad[lson[p]] = q;
        }
        rson[q] = rson[p];
        dad[rson[p]] = q;
    }
    dad[q] = dad[p];
    if (rson[dad[p]] == p)
        rson[dad[p]] = q;
    else
        lson[dad[p]] = q;
    dad[p] = NIL;
}

int romc_encode(uint8_t data[], int size, uint8_t out[])
// function patched to produce gba bios decompression function processable data
{
    int i, c, len, r, s, last_match_length, code_buf_ptr;
	int out_index = 0;
	int in_index = 0;
    uint8_t code_buf[17], mask;

    if (size < 1) {
        return -1;
    }

	if ((size % fourmbit) > 0) {
            int new_size = fourmbit * ((size % fourmbit) + 1);
			data = realloc(data, new_size);
            if (data == NULL)
                return -1;
            memset(data + size, 0, new_size - size);
            size = new_size;
    }

    // write 32 bit header needed for GBA BIOS function
    // Bit 0-3   Reserved
    // Bit 4-7   Compressed type (must be 1 for LZ77)
    // Bit 8-31  Size of decompressed data


    uint8_t tmp[] = {size / fourmbit, 0x00, 0x00, 0x01};

    for (i = 0; i < 4; i++)
        out[out_index++] = tmp[i];

    init_tree();     /* initialize trees */
    code_buf[0] = 0; /* code_buf[1..16] saves eight units of code, and
           code_buf[0] works as eight flags, "0" representing that the unit
           is an unencoded letter (1 byte), "1" a position-and-length pair
           (2 bytes).  Thus, eight units require at most 16 bytes of code. */
    code_buf_ptr = 1;
    mask = 0x80; // für GBA fangen wir mit MSB an
    s = 0;
    r = N - F; // 4078

    for (i = s; i < r; i++)
        text_buf[i] = 0xff; /* Clear the buffer with any character that will appear often. */
    for (len = 0; len < F && in_index < size; len++)
        text_buf[r + len] = data[in_index++]; /* Read F bytes into the last F bytes of
               the buffer */
    if ((textsize = len) == 0)
        return -1; /* text of size zero */
    for (i = 1; i <= F; i++)
        insert_node(r - i); /* Insert the F strings,
each of which begins with one or more 'space' characters.  Note
the order in which these strings are inserted.  This way,
degenerate trees will be less likely to occur. */
    insert_node(r);         /* Finally, insert the whole string just read.  The
                   global variables match_length and match_position are set. */

    // kompressions schleife

    do {
        if (match_length > len)
            match_length = len; /* match_length
may be spuriously long near the end of text. */

        // nicht komprimieren
        if (match_length <= THRESHOLD) {
            match_length = 1; /* Not long enough match.  Send one byte. */
            // code_buf[0] |= mask;  /* 'send one byte' flag */
            code_buf[code_buf_ptr++] = text_buf[r]; /* Send uncoded. */
        } else
        // komprimieren
        {
            code_buf[0] |= mask; // flag "komprimiert" setzen

            // Bit 0-3   Disp MSBs
            // Bit 4-7   Number of bytes to copy (minus 3)
            // Bit 8-15  Disp LSBs

            code_buf[code_buf_ptr++] =
                (unsigned char)(((r - match_position - 1) >> 8) & 0x0f) | ((match_length - (THRESHOLD + 1)) << 4);

            code_buf[code_buf_ptr++] = (unsigned char)((r - match_position - 1) & 0xff);
            /* Send position and length pair. Note match_length > THRESHOLD. */
        }

        // mask shift
        if ((mask >>= 1) == 0) {               /* Shift mask right one bit. */
            for (i = 0; i < code_buf_ptr; i++) /* Send at most 8 units of */
                out[out_index++] = code_buf[i];    /* code together */
            codesize += code_buf_ptr;
            code_buf[0] = 0;
            code_buf_ptr = 1;
            mask = 0x80;
        }

        last_match_length = match_length;
        for (i = 0; i < last_match_length && in_index < size; i++) {
			c = data[in_index++];
            delete_node(s);  /* Delete old strings and */
            text_buf[s] = c; /* read new bytes */
            if (s < F - 1)
                text_buf[s + N] = c; /* If the position is
        near the end of buffer, extend the buffer to make
        string comparison easier. */
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            /* Since this is a ring buffer, increment the position
               modulo N. */
            insert_node(r); /* Register the string in text_buf[r..r+F-1] */
        }
        while (i++ < last_match_length) { /* After the end of text, */
            delete_node(s);               /* no need to read, but */
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            if (--len)
                insert_node(r); /* buffer may not be empty. */
        }
    } while (len > 0); /* until length of string to be processed is zero */

    if (code_buf_ptr > 1) { /* Send remaining code. */
        for (i = 0; i < code_buf_ptr; i++)
            out[out_index++] = code_buf[i];
        codesize += code_buf_ptr;
    }

    // pad output with zeros to make it a multiply of 4
    if (codesize % 4)
        for (i = 0; i < 4 - (codesize % 4); i++, out_index++) {}

    codesize += 4 - (codesize % 4);

	return out_index;
}

void romc_decode(const uint8_t* data, size_t size, uint8_t* out)
{
    int i, j, k, r, c, z;
    unsigned int flags;
    u32 decomp_size;
    u32 in_index = 0;
    u32 out_index = 0;

    unsigned char tmp[4];
	for(i=0; i<4; i++) tmp[i] = data[in_index++];
	decomp_size = tmp[0] * fourmbit;

    for (i = 0; i < N - F; i++) text_buf[i] = 0xff;
    r = N - F;  flags = z = 7;
    for ( ; ; ) {
        flags <<= 1;
        z++;
        if (z == 8) {
            if (in_index >= size) break;
            flags = data[in_index++];
            z = 0;
        }
        if (!(flags&0x80)) {
            if (in_index >= size) break;
            c = data[in_index++];
            if(out_index < decomp_size) out[out_index++] = c;
            text_buf[r++] = c;  r &= (N - 1);
        } else {
            if (in_index + 1 >= size) break;
            i = data[in_index++];
            j = data[in_index++];
            j = j | ((i<<8)&0xf00);
            i = ((i>>4)&0x0f) + THRESHOLD;
            for (k = 0; k <= i; k++) {
                c = text_buf[(r-j-1) & (N - 1)];
                if(out_index < decomp_size) out[out_index++] = c;
                text_buf[r++] = c;  r &= (N - 1);
            }
        }
    }
}
