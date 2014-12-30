// -*- C++ -*-
#include <stdio.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <cilk/cilk.h>

#include "reducer_vector.h"

#define DEBUG false

// Assert a condition that should be true during the test.
unsigned test_status = 0;
#define L_ __LINE__
const int fromline = 0;

// Introduce error in gcc/*nix format.
#define ERROR_INTRO(file,line) file << ":" << line << ": error:"

void test_assert_failure(int line, int fromline, const char* cond)
{
  std::cout << ERROR_INTRO(__FILE__, line)
	    << " Assert failed: " << cond << std::endl;
  
  if (fromline)
    std::cout << ERROR_INTRO(__FILE__, fromline)
	      << "     called from here" << std::endl;
  ++test_status;
}

#define TEST_ASSERT(c) do { if (!(c)) {					\
      test_assert_failure(__LINE__, fromline, #c); } } while (false)

void fill_monoid_val(hypervector<char> *m,
                     const char* s, std::size_t len)
{
  for (int i = 0; i < len; ++i) {
    m->push_back(s[i]);
  }
}

void test_monoid_val(int fromline, const char* s, std::vector<char> *expected)
{
  const std::size_t len = std::strlen(s);
  std::vector<char> vec;

  // Orthoganol perturbation: Two-way split
  for (std::size_t split = 0; split < len; ++split) {
    //fprintf(stderr, "split = %d\n", split);

    hypervector<char> *left = new hypervector<char>();
    hypervector<char> *right = new hypervector<char>();
    
    fill_monoid_val(left, s, split);
    fill_monoid_val(right, s + split, len - split);

    reducer_basic_vector<char>::Monoid::reduce(left, right);
    delete right;
    TEST_ASSERT(left->get_vec() == *expected);

    delete left;
    //fprintf(stderr, "left deleted\n");
    //fprintf(stderr, "right deleted\n");
  }

  // Orthoganol perturbation: Three-way splits
  for (std::size_t split1 = 0; split1 < len; ++split1) {
    for (std::size_t split2 = split1; split2 < len; ++split2) {
      //fprintf(stderr, "split1 = %d, split2 = %d\n", split1, split2);
      hypervector<char> *leftA   = new hypervector<char>();
      hypervector<char> *middleA = new hypervector<char>();
      hypervector<char> *rightA  = new hypervector<char>();
      hypervector<char> *leftB   = new hypervector<char>();
      hypervector<char> *middleB = new hypervector<char>();
      hypervector<char> *rightB  = new hypervector<char>();
      
      // Split string into three, each into its own paren_counter
      fill_monoid_val(leftA,   s,          split1);
      fill_monoid_val(leftB,   s,          split1);
      fill_monoid_val(middleA, s + split1, split2 - split1);
      fill_monoid_val(middleB, s + split1, split2 - split1);
      fill_monoid_val(rightA,  s + split2, len - split2);
      fill_monoid_val(rightB,  s + split2, len - split2);
      
      // Combine monoids (left + middle) + right
      reducer_basic_vector<char>::Monoid::reduce(leftA, middleA);
      delete middleA;
      reducer_basic_vector<char>::Monoid::reduce(leftA, rightA);
      delete rightA;
      if (DEBUG) {
	printf("leftA->get_vec() = ");
	vec = leftA->get_vec();
	for (int i = 0; i < vec.size(); ++i) {
	  printf("%c", vec[i]);
	}
	printf("\n");
      }
      TEST_ASSERT(leftA->get_vec() == *expected);
      TEST_ASSERT(leftA->get_vec() == *expected);
      
      // Combine monoids left + (middle + right)
      reducer_basic_vector<char>::Monoid::reduce(middleB, rightB);
      delete rightB;
      reducer_basic_vector<char>::Monoid::reduce(leftB, middleB);
      delete middleB;
      //printf("reductions complete for B\n");
      if (DEBUG) {
	printf("leftB->get_vec() = ");
	vec = leftB->get_vec();
	for (int i = 0; i < vec.size(); ++i) {
	  printf("%c", vec[i]);
	}
	printf("\n");
      }
      TEST_ASSERT(leftB->get_vec() == *expected);
      TEST_ASSERT(leftB->get_vec() == *expected);

      delete leftA;
      delete leftB;
    } // split2
  } // split1
}

void get_expected_vector(std::vector<char> *expected, const char* s, std::size_t len)
{
  expected->clear();
  expected->reserve(len);

  for (int i = 0; i < len; ++i)
    expected->push_back(s[i]);
}

int
main(int argc, char* argv[])
{
  struct testvec {
    int         line_;
    const char* s_;
  } const test_data[] = {
    //line string                                    
    //==== ==========================================
    { L_,  ""                                        }, // level 0 // 0
    
    { L_,  ")"                                       }, // level 1 // 1
    { L_,  "("                                       },            // 2
    { L_,  "x"                                       },            // 3
    
    { L_,  "ab"                                      }, // level 2 // 4
    { L_,  "bc"                                      },            // 5
    { L_,  "cd"                                      },            // 6
    { L_,  "de"                                      },            // 7
    { L_,  "ef"                                      },            // 8
    { L_,  "fg"                                      },            // 9
    { L_,  "gh"                                      },            // 10
    { L_,  "hi"                                      },            // 11
    { L_,  "ij"                                      },            // 12
    
    { L_,  "abc"                                     }, // level 3 // 13
    { L_,  "def"                                     },            // 14
    { L_,  "ghi"                                     },            // 15
    { L_,  "jkl"                                     },            // 16
    { L_,  "mno"                                     },            // 17
    { L_,  "pqr"                                     },            // 18
    { L_,  "stu"                                     },            // 19
    { L_,  "vwx"                                     },            // 20
    { L_,  "yz1"                                     },            // 21
    
    { L_,  "abcd"                                    }, // level 4 // 22
    { L_,  "bcde"                                    },            // 23
    { L_,  "cdef"                                    },            // 24
    { L_,  "defg"                                    },            // 25
    { L_,  "efgh"                                    },            // 26
    { L_,  "fghi"                                    },            // 27
    { L_,  "ghij"                                    },            // 28
    { L_,  "hijk"                                    },            // 29
    { L_,  "ijkl"                                    },            // 30
    { L_,  "jklm"                                    },            // 31
    { L_,  "klmn"                                    },            // 32
    { L_,  "lmno"                                    },            // 33
    { L_,  "mnop"                                    },            // 34
    { L_,  "nopq"                                    },            // 35
    { L_,  "opqr"                                    },            // 36
    { L_,  "pqrs"                                    },            // 37
    
    { L_,  "DEADBEEF"                                }, // ad-hoc  // 38
    { L_,  "ASSERT(leftB.is_balanced() == expected)" },            // 39
    { L_,  "(car (car (car (cdr (cdr x)))))"         },            // 40
    { L_,  "(car (car (car (cdr)(cdr x)))))"         },            // 41
    { L_,  "(car (car (car (cdr (cdr x))))"          },            // 42
    { L_,  "((car (car (car (cdr (cdr x))))"         },            // 43
  };
  
  const std::size_t test_data_size = sizeof(test_data) / sizeof(testvec);
  std::vector<char> *expected = new std::vector<char>();

  for (std::size_t i = 0; i < test_data_size; ++i) {
    //fprintf(stderr, "Test iteration %d\n", i);
    const int         line     = test_data[i].line_;
    const char *const s        = test_data[i].s_;
    get_expected_vector(expected, s, std::strlen(s));
    test_monoid_val(line, s, expected);
    std::cout << "Test " << i << " done" << std::endl;
  }

  // Test case: test merging a bunch of hypervectors
  const int         line = L_;
  const char *const s =
"To be or not to be: that is the question:\n\
Whether 'tis nobler in the mind to suffer\n\
The slings and arrows of outrageous fortune,\n\
Or to take arms against the sea of troubles\n\
And, by opposing, end them. To die: to sleep;\n\
No more; and by a sleep to say we end\n\
The heart-ache and the thousand natural shocks\n\
That flesh is heir to, 'tis a consummation\n\
Devoutly to be wish'd. To die, to sleep\n\
To sleep: perchance to dream: ay, there's the rub;\n\
For in that sleep of death what dreams may come\n\
When we have shuffled off this mortal coil,\n\
Must give us pause: there's the respect\n\
That makes calamity of so long life;\n\
For who would bear the whips and scorns of time,\n\
The oppressor's wrong, the proud man's contumely,\n\
The pangs of despised love, the law's delay,\n\
The insolence of office and the spurns\n\
That patient merit of the unworthy takes,\n\
When he himself might his quietus make\n\
With a bare bodkin? who would fardels bear,\n\
To grunt and sweat under a weary life,\n\
But that the dread of something after death,\n\
The undiscover'd country from whose bourn\n\
No traveller returns, puzzles the will\n\
And makes us rather bear those ills we have\n\
Than fly to others that we know not of?\n\
Thus conscience does make cowards of us all;\n\
And thus the native hue of resolution\n\
Is sicklied o'er with the pale cast of thought,\n\
And enterprises of great pith and moment\n\
With this regard their currents turn awry,\n\
And lose the name of action. - Soft you now!\n\
The fair Ophelia! Nymph, in thy orisons\n\
Be all my sins remember'd.\n\
The sun was shining on the sea,\n\
Shining with all his might:\n\
He did his very best to make\n\
The billows smooth and bright--\n\
And this was odd, because it was\n\
The middle of the night.\n\
The moon was shining sulkily,\n\
Because she thought the sun\n\
Had got no business to be there\n\
After the day was done--\n\
\"It's very rude of him,\" she said,\n\
\"To come and spoil the fun!\"\n\
The sea was wet as wet could be,\n\
The sands were dry as dry.\n\
You could not see a cloud, because\n\
No cloud was in the sky:\n\
No birds were flying overhead--\n\
There were no birds to fly.\n\
The Walrus and the Carpenter\n\
Were walking close at hand;\n\
They wept like anything to see\n\
Such quantities of sand:\n\
\"If this were only cleared away,\"\n\
They said, \"it would be grand!\"\n\
\"If seven maids with seven mops\n\
Swept it for half a year.\n\
Do you suppose,\" the Walrus said,\n\
\"That they could get it clear?\"\n\
\"I doubt it,\" said the Carpenter,\n\
And shed a bitter tear.\n\
\"O Oysters, come and walk with us!\"\n\
The Walrus did beseech.\n\
\"A pleasant walk, a pleasant talk,\n\
Along the briny beach:\n\
We cannot do with more than four,\n\
To give a hand to each.\"\n\
The eldest Oyster looked at him,\n\
But never a word he said:\n\
The eldest Oyster winked his eye,\n\
And shook his heavy head--\n\
Meaning to say he did not choose\n\
To leave the oyster-bed.\n\
But four young Oysters hurried up,\n\
All eager for the treat:\n\
Their coats were brushed, their faces washed,\n\
Their shoes were clean and neat--\n\
And this was odd, because, you know,\n\
They hadn't any feet.\n\
Four other Oysters followed them,\n\
And yet another four;\n\
And thick and fast they came at last,\n\
And more, and more, and more--\n\
All hopping through the frothy waves,\n\
And scrambling to the shore.\n\
The Walrus and the Carpenter\n\
Walked on a mile or so,\n\
And then they rested on a rock\n\
Conveniently low:\n\
And all the little Oysters stood\n\
And waited in a row.\n\
\"The time has come,\" the Walrus said,\n\
\"To talk of many things:\n\
Of shoes--and ships--and sealing-wax--\n\
Of cabbages--and kings--\n\
And why the sea is boiling hot--\n\
And whether pigs have wings.\"\n\
\"But wait a bit,\" the Oysters cried,\n\
\"Before we have our chat;\n\
For some of us are out of breath,\n\
And all of us are fat!\"\n\
\"No hurry!\" said the Carpenter.\n\
They thanked him much for that.\n\
\"A loaf of bread,\" the Walrus said,\n\
\"Is what we chiefly need:\n\
Pepper and vinegar besides\n\
Are very good indeed--\n\
Now if you're ready, Oysters dear,\n\
We can begin to feed.\"\n\
\"But not on us!\" the Oysters cried,\n\
Turning a little blue.\n\
\"After such kindness, that would be\n\
A dismal thing to do!\"\n\
\"The night is fine,\" the Walrus said.\n\
\"Do you admire the view?\n\
\"It was so kind of you to come!\n\
And you are very nice!\"\n\
The Carpenter said nothing but\n\
\"Cut us another slice\n\
I wish you were not quite so deaf--\n\
I've had to ask you twice!\"\n\
\"It seems a shame,\" the Walrus said,\n\
\"To play them such a trick,\n\
After we've brought them out so far,\n\
And made them trot so quick!\"\n\
The Carpenter said nothing but\n\
\"The butter's spread too thick!\"\n\
\"I weep for you,\" the Walrus said:\n\
\"I deeply sympathize.\"\n\
With sobs and tears he sorted out\n\
Those of the largest size,\n\
Holding his pocket-handkerchief\n\
Before his streaming eyes.\n\
\"O Oysters,\" said the Carpenter,\n\
\"You've had a pleasant run!\n\
Shall we be trotting home again?'\n\
But answer came there none--\n\
And this was scarcely odd, because\n\
They'd eaten every one.";

  const std::size_t len = std::strlen(s);
  get_expected_vector(expected, s, len);

  // Make 16 hypervectors
  hypervector<char>* *hv = new hypervector<char>*[16];
  for (int i = 0; i < 16; ++i) {
    hv[i] = new hypervector<char>();
  }

  for (int i = 1; i < 16; i += 2) {
    // Distribute poetry among the hypervectors
    if (i < 15)
      fill_monoid_val(hv[i], s + 296*i, 296);
    else
      fill_monoid_val(hv[i], s + 296*15, len - (296*15));
  }
  // Combine some hypervectors
  for (int i = 0; i < 16; i += 2) {
    reducer_basic_vector<char>::Monoid::reduce(hv[i], hv[i+1]);
  }

  for (int i = 0; i < 16; i += 2) {
    // Put the rest of poetry into hypervectors
      fill_monoid_val(hv[i], s + 296*i, 296);
  }

  // Combine the hypervectors in a binary-tree fashion
  for (int step = 4; step <= 16; step *= 2) {
    for (int i = 0; i < 16; i += step) {
      reducer_basic_vector<char>::Monoid::reduce(hv[i], hv[i+(step/2)]);
    }
  }

  // Test twice to ensure get_vec() does the right thing the second time
  TEST_ASSERT(hv[0]->get_vec() == *expected);
  TEST_ASSERT(hv[0]->get_vec() == *expected);
  //std::cout << "Test Hamlet done." << std::endl;

  std::cout << "done." << std::endl; 
  return test_status;
}
