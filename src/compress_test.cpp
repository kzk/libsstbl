#include <compress.h>

#include <gtest/gtest.h>

using namespace std;

TEST(compress, zlib) {
  for (int i = 0; i < 100; i++) {
    string s;
    int len = 56 * 1024 + rand() % (12 * 1024);
    for (int j = 0; j < len; j++)
      s += 'a' + rand() % 26;
    int sp = 0;
    char *cmp = sscodec_zlibcompress(s.c_str(), len, &sp);
    ASSERT_TRUE(cmp != NULL);
    ASSERT_TRUE(sp > 0);
    int dsp = 0;
    char *dcmp = sscodec_zlibdecompress(cmp, sp, &dsp);
    ASSERT_TRUE(dcmp != NULL);
    ASSERT_EQ(len, dsp);
    ASSERT_EQ(0, memcmp(s.c_str(), dcmp, len));
  }
}

TEST(compress, lzo) {
  for (int i = 0; i < 100; i++) {
    string s;
    int len = 1 * 1024 + rand() % (1 * 1024);
    for (int j = 0; j < len; j++)
      s += 'a' + rand() % 26;
    int sp = 0;
    char *cmp = sscodec_lzocompress(s.c_str(), len, &sp);
    ASSERT_TRUE(cmp != NULL);
    ASSERT_TRUE(sp > 0);
    int dsp = 0;
    char *dcmp = sscodec_lzodecompress(cmp, sp, &dsp);
    ASSERT_TRUE(dcmp != NULL);
    ASSERT_EQ(len, dsp);
    ASSERT_EQ(0, memcmp(s.c_str(), dcmp, len));
  }
}

