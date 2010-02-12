#include <compress/blkhash.h>
#include <compress/rollinghash.h>

#include <gtest/gtest.h>

using namespace std;

class BlockHashTestFixture : public testing::Test {
protected:
  void SetUp() {
    GenerateData();
    bhash = blkhashnew(data.c_str(), data.size());
    ASSERT_TRUE(bhash != NULL);
    blksiz = bhash->blksiz;
    rhash = rollinghashnew(blksiz);
    ASSERT_TRUE(rhash != NULL);
    for (unsigned int i = 0; i < data.size(); i++) {
      int r = blkhashaddhash(bhash, i, rollinghashdohash(rhash, data.c_str() + i));
      ASSERT_EQ(0, r);
    }
  }
  void TearDown() {
    blkhashdel(bhash);
  }
  virtual void GenerateData() = 0;
  string data;
  size_t blksiz;
  BLKHASH *bhash;
  ROLLINGHASH *rhash;
};

/*-----------------------------------------------------------------------------
 * First and Next Match
 */
class BlockHashFirstAndNextMatchTestFixture : public BlockHashTestFixture {
public:
  virtual void GenerateData() {
    for (unsigned int i = 0; i < 10000; i++)
      data += 'a' + rand() % 26;
    data += data;
  }
};

TEST_F(BlockHashFirstAndNextMatchTestFixture, none) {
}

TEST_F(BlockHashFirstAndNextMatchTestFixture, firstmatch_and_nextmatch) {
  for (unsigned int i = 0; i < data.size(); i++) {
    if (i % blksiz != 0) continue;
    if (i + blksiz >= data.size()) break;

    vector<int> cands;
    uint32_t hash = rollinghashdohash(rhash, data.c_str() + i);
    int blknum = blkhashfindfirstmatch(bhash, hash, data.c_str() + i);
    ASSERT_TRUE(blknum >= 0);
    cands.push_back(blknum);
    for (; blknum >= 0;) {
      blknum = blkhashfindnextmatch(bhash, blknum, data.c_str() + i);
      if (blknum >= 0)
        cands.push_back(blknum);
    }
    ASSERT_TRUE(cands.size() >= 1);
    ASSERT_EQ(1, count(cands.begin(), cands.end(), i / blksiz));

    // generate non-matching data
    string s;
    do {
      for (unsigned int i = 0; i < blksiz; i++)
        s += 'a' + rand() % 26;
    } while (data.find(s) != string::npos);
    blknum = blkhashfindfirstmatch(bhash, hash, s.c_str());
    ASSERT_EQ(-1, blknum);
  }
}

/*-----------------------------------------------------------------------------
 * Best Match
 */
class BlockHashBestMatchTest1Fixture : public BlockHashTestFixture {
public:
  virtual void GenerateData() {
    for (unsigned int i = 0; i < (1<<12); i++)
      data += 'a' + rand() % 26;
  }
};

TEST_F(BlockHashBestMatchTest1Fixture, allmatch) {
  int r;
  for (unsigned int i = 0; i < data.size(); i++) {
    if (i + blksiz > data.size()) break;
    if (i % blksiz != 0) continue;
    BLKHASHMATCH match;
    r = blkhashfindbestmatch(bhash, rollinghashdohash(rhash, data.c_str() + i),
                             data.c_str() + i, data.c_str(), data.size(), &match);
    ASSERT_TRUE(r >= 0);
    ASSERT_EQ(data.size(), match.size);
  }
}
