#include <compress/blkhash.h>
#include <compress/rollinghash.h>

#include <gtest/gtest.h>

using namespace std;

class BlockHashTestFixture : public testing::Test {
protected:
  void SetUp() {
    for (unsigned int i = 0; i < 10000; i++)
      data += 'a' + rand() % 26;
    data += data;
    bhash = blkhashnew(data.c_str(), data.size());
    ASSERT_TRUE(bhash != NULL);
    blksiz = bhash->blksiz;
    rhash = rollinghashnew(blksiz);
    ASSERT_TRUE(rhash != NULL);
  }
  void TearDown() {
    blkhashdel(bhash);
  }
  string data;
  size_t blksiz;
  BLKHASH *bhash;
  ROLLINGHASH *rhash;
};

TEST_F(BlockHashTestFixture, none) {
}

TEST_F(BlockHashTestFixture, simple) {
  for (unsigned int i = 0; i < data.size(); i++) {
    int r = blkhashaddhash(bhash, i, rollinghashdohash(rhash, data.c_str() + i));
    ASSERT_EQ(0, r);
  }
  for (unsigned int i = 0; i < data.size(); i++) {
    if (i % blksiz != 0) continue;
    if (i + blksiz >= data.size()) break;

    vector<int> cands;
    int blknum = blkhashfindfirstmatch(bhash, rollinghashdohash(rhash, data.c_str() + i),
                                       data.c_str() + i);
    ASSERT_TRUE(blknum >= 0);
    cands.push_back(blknum);
    for (; blknum >= 0;) {
      blknum = blkhashfindnextmatch(bhash, blknum, data.c_str() + i);
      if (blknum >= 0)
        cands.push_back(blknum);
    }
    ASSERT_TRUE(cands.size() >= 1);
    ASSERT_EQ(1, count(cands.begin(), cands.end(), i / blksiz));
  }
}
