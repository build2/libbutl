// file      : tests/move-only-function/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <memory>  // unique_ptr
#include <utility> // move()

#include <libbutl/move-only-function.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;

static int
func (int v)
{
  return v + 1;
}

struct functor
{
  int i;

  int
  operator() (int v)
  {
    return v + i;
  }
};

int
main ()
{
  using butl::move_only_function_ex;

  // Attempt to copy-construct or copy-assign should not compile.
  // Also check non-collable.
  //
#if 0
  {
    using ft = move_only_function_ex<int (int)>;
    ft f;
    ft f2 (f);
    ft f3; f3 = f;
    ft f4 (123);
  }
#endif

  // NULL.
  //
  {
    using ft = move_only_function_ex<int (int)>;

    ft f1;
    assert (!f1);

    ft f2 (nullptr);
    assert (f2 == nullptr);

    f1 = func;
    assert (f1 != nullptr);
    f1 = nullptr;
    assert (!f1);

    int (*f) (int) = nullptr;
    f2 = f;
    assert (!f2);
  }

  // Function.
  //
  {
    using ft = move_only_function_ex<int (int)>;

    ft f (func);

    assert (f (1) == 2);

    ft f1 (move (f));
    assert (!f);
    assert (f1 (1) == 2);

    f = &func;

    assert (f (1) == 2);

    assert (f.target<int (*) (int)> () != nullptr);
    assert (f1.target<int (*) (int)> () != nullptr);
  }

  // Functor.
  //
  {
    using ft = move_only_function_ex<int (int)>;

    ft f (functor {1});

    assert (f (1) == 2);

    ft f1 (move (f));
    assert (!f);
    assert (f1 (1) == 2);

    f = functor {2};

    assert (f (1) == 3);

    assert (ft (functor {1}).target<functor> () != nullptr);
  }

  // Lambda.
  //
  {
    using ft = move_only_function_ex<int (int)>;

    ft f ([p = unique_ptr<int> (new int (1))] (int v)
          {
            return *p + v;
          });

    assert (f (1) == 2);

    ft f1 (move (f));
    assert (!f);
    assert (f1 (1) == 2);

    f = ([p = unique_ptr<int> (new int (2))] (int v)
         {
           return *p + v;
         });

    assert (f (1) == 3);
  }

  // Void result.
  //
  {
    using ft = move_only_function_ex<void (int)>;

    ft f ([] (int v)
          {
            assert (v == 1);
          });

    f (1);
    ft f1 (move (f));
    f1 (1);
  }
}
