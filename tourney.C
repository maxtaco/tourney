#include "async.h"
#include "vec.h"
#include "qhash.h"
#include "parseopt.h"

class game_split_t : public vec<size_t> {
public:
  game_split_t (const game_split_t &x) : vec<size_t> (x), _tot (0) {}
  game_split_t () : _tot (0) {}

  ptr<game_split_t> copy () const 
  { return New refcounted<game_split_t> (*this); }

  void add (size_t i) {
    _tot += i;
    push_back (i);
  }

  void subtract () { _tot -= pop_back (); }
  int tot () const { return _tot; }
  size_t n_games () const { return size (); }
  size_t _tot;
};

class game_t {
public:

  void fill_sizes () { fill_sizes (0, New refcounted<game_split_t> ()); }

  void fill_sizes (int n, ptr<game_split_t> v)
  {
    if (n >= _sets) return;
    for (int i = _min; i <= _max; i++) {
      v->add (i);
      _valid_sizes.insert (v->tot (), v->copy ());
      if (v->tot () > _allmax) { _allmax = v->tot (); }
      fill_sizes (n + 1, v);
      v->subtract ();
    }
  }

  game_t (str nm, char c, int n, int x, int s, int ppq) : 
    _name (nm), _ch (c), _min (n), _max (x), _sets (s), _per_person_quota (ppq),
    _allmax (0)
  {
    static int global_id;
    _id = global_id++;
    fill_sizes ();
  }


  int id () const { return _id; }
  char ch () const { return _ch; }
  str _name;
  char _ch;
  int _min, _max;
  int _sets;
  int _id;
  int _per_person_quota;
  int _allmax;
  qhash<size_t, ptr<game_split_t> > _valid_sizes;
};

template<class T> void shuffle (vec<T> &v)
{
  size_t n = v.size ();
  while (n > 1) {
    size_t i = rand () % n;
    n --;
    T tmp = v[i];
    v[i] = v[n];
    v[n] = tmp;
  }
}

template<class T> void array_init (vec<T> &v, size_t sz, T val)
{ 
  v.setsize (sz);
  for (size_t i = 0; i < v.size (); i++) { v[i] = val; } 
}


class tourney_constants_t {
public:
  tourney_constants_t (int n) : _n_players (n), _n_rounds (0) {}
  void add_game (str n, char c, int mn, int mx, int sets, int ppq)
  {
    _games.push_back (New game_t (n, c, mn, mx, sets, ppq));
  }

  void done ()
  {
    _n_rounds = 0;
    for (size_t i = 0; i < _games.size (); i++) {
      _n_rounds += _games[i]->_per_person_quota;
    }
    make_permutations ();
  }

  void make_permutations () 
  { make_permutations (vec<size_t> (), bhash<int> ()); }

  void make_permutations (vec<size_t> v, bhash<int> h)
  {
    size_t sz = _games.size ();
    if (v.size () == sz) { 
      _permutations.push_back (v); 
    } else {
      for (size_t i = 0; i < sz; i++) {
	if (!h[i]) {
	  h.insert (i);
	  v.push_back (i);
	  make_permutations (v, h);
	  v.pop_back ();
	  h.remove (i);
	}
      }
    }
  }

  const vec<size_t> &random_permutation ()
  {
    return _permutations[ rand () % _permutations.size () ];
  }

  int n_players () const { return _n_players; }
  int n_games () const { return _games.size (); }

  int n_rounds () const { return _n_rounds; }

  int _n_players;
  int _n_rounds;
  vec<game_t *> _games;
  
  vec<vec<size_t> > _permutations;
};

class player_t {
public:
  player_t (int nrounds, int ngames) {
    array_init<game_t *> (_rounds, nrounds, NULL);
    array_init<int> (_game_counts, ngames, 0);
    static int global_id ;
    _id = global_id ++;
  }

  int id () const { return _id; }
  ~player_t () {}

  void push_round (game_t *g) { 
    _rounds.push_back (g);
    _game_counts[g->id()] ++;
  }
  
  void pop_round () {
    game_t *g = _rounds.pop_back ();
    _game_counts[g->id()] --;
  }

  bool can_play (game_t *g, const tourney_constants_t &tc) {
    int id = g->id ();
    return _game_counts[id] + 1 <= tc._games[id]->_per_person_quota;
  }
  
  vec<game_t *> _rounds;
  vec<int> _game_counts;
  
  int _id;
};

class schedule_t : virtual refcount {
public:
  schedule_t (const tourney_constants_t &tc) {
    array_init<game_t *> (_player_map, tc.n_players (), NULL);
    array_init<int> (_game_count, tc.n_games (), 0);
  }

  void add (player_t *p, game_t *g) {
    _player_map[p->id ()] = g;
    _game_count[g->id ()] ++;
  }

  void subtract (player_t *p, game_t *g) {
    _player_map[p->id ()] = NULL;
    _game_count[g->id ()] --;
  }

  str output (const tourney_constants_t &tc) const
  {
    strbuf b;
    vec<vec<int> > game_sequences;

    // First figure out which games to break the players up into,
    // and then populate those games in random order.
    for (size_t i = 0; i < _game_count.size (); i++) {
      vec<int> games;
      game_t *g = tc._games[i];
      int c = _game_count[i];
      ptr<game_split_t> gs = *g->_valid_sizes[c];
      for (size_t j = 0; j < gs->size (); j++) {
	for (size_t k = 0; k < (*gs)[j]; k++) {
	  games.push_back (j+1);
	}
      }
      shuffle (games);
      game_sequences.push_back (games);
    }

    // Now iterate over all of the players, popping off the random game
    // slots.
    for (size_t i = 0; i < _player_map.size (); i++) {
      if (i > 0) b << " ";
      str s;
      if (_player_map[i]) { 
	game_t *g = _player_map[i];
	s = strbuf ("%c%d", g->ch (), game_sequences[g->id ()].pop_back ());
      }
      else { s = "XX"; }
      b << s;
    }

    b << "   ";

    // Finaly, report on how many of each type of game are being played.
    for (size_t i = 0; i < _game_count.size (); i++) {
      game_t *g = tc._games[i];
      int c = _game_count[i];
      int ng = (*g->_valid_sizes[c])->n_games ();
      b.fmt (" %c:%d", g->ch (), ng);
    }
    return b;
  }

  bool valid_sizes (const tourney_constants_t &c) 
  {
    for (size_t i = 0; i < _game_count.size (); i++) {
      if (!c._games[i]->_valid_sizes[_game_count[i]]) {
	return false;
      }
    }
    return true;
  }

  bool enough_game_sets (game_t *g, const tourney_constants_t &c)
  {
    int id = g->id ();
    return _game_count[id] + 1 <= c._games[id]->_allmax;
  }
  
  vec<game_t *> _player_map;
  vec<int> _game_count;
};


class tournament_t {
public:
  tournament_t (tourney_constants_t &c) : 
    _tc (c), _solved (false), _aborted (false), _n_schedules (0) 
  {
    _rounds.setsize (c.n_rounds ());
    _players.setsize (c.n_players ());
    for (size_t i = 0; i < _players.size (); i++) {
      _players[i] = New player_t (c.n_rounds (), c.n_games ());
    }
  }

  void schedule () { schedule (0); }

  void assign (ptr<schedule_t> a) 
  {
    for (size_t i = 0; i < _players.size (); i++ ) {
      _players[i]->push_round (a->_player_map[i]);
    }
  }

  void unassign ()
  {
    for (size_t i = 0; i < _players.size (); i++) {
      _players[i]->pop_round ();
    }
  }


  bool is_valid (ptr<schedule_t> sched, player_t *p, game_t *g) 
  { 
    return p->can_play (g, _tc) && sched->enough_game_sets (g, _tc);
  }

  void sched_generate (ptr<schedule_t> sched, size_t rn, size_t player_pos) 
  {
    if (player_pos == _players.size ()) {
      _n_schedules ++;
      if (_n_schedules > 2000000) {
	_aborted = true;
      }
      if (sched->valid_sizes (_tc)) {
	_rounds[rn] = sched;
	assign (sched);
	schedule (rn + 1);
	if (!_solved) unassign ();
      }
    } else {
      vec<size_t> perm = _tc.random_permutation ();
      for (size_t ig = 0; ig < _tc._games.size (); ig++) {
	game_t *g = _tc._games[perm[ig]];
	player_t *p = _players[player_pos];
	if (is_valid (sched, p, g)) {
	  sched->add (p, g);
	  sched_generate (sched, rn, player_pos + 1);
	  if (_solved || _aborted) 
	    return;
	  sched->subtract (p, g);
	}
      }
    }
  }

  void reset ()
  {
    _aborted = false;
    _solved = false;
    _n_schedules = 0;
  }

  bool solved () const { return _solved; }
  
  void schedule (size_t rn) {
    if (rn == _rounds.size ()) {
      _solved = true;
    } else {
      ptr<schedule_t> sched = New refcounted<schedule_t> (_tc);
      sched_generate (sched, rn, 0);
    }
  }

  void print ()
  {
    strbuf h;
    h << "  ";
    for (size_t i = 0; i < _players.size (); i++) {
      h.fmt (" %2d", int (i+ 1));
    }
    warn << h << "\n";

    for (size_t i = 0; i < _rounds.size (); i++) {
      str s = _rounds[i]->output (_tc);
      warn << i << ": " << s << "\n";
    }
    warn << "----------------------------------------------------------\n";
    warn << "(done with " << _n_schedules << " schedule possibilities)\n";
  }
  
  tourney_constants_t &_tc;
  vec<ptr<schedule_t > > _rounds;
  vec<player_t *> _players;
  bool _solved, _aborted;
  int _n_schedules;
};

int main(int argc, char *argv[])
{
  srand (time (NULL) ^ getpid ());
  int n_players = 0;
  if (argc != 2 || !convertint (argv[1], &n_players)) {
    fatal << "usage: " << argv[0] << " <n-players>\n";
  }

  tourney_constants_t tc (n_players);
  tc.add_game ("boggle", 'B', 3, 5, 4, 1);
  tc.add_game ("bananagrams", 'Z', 3, 5, 4, 1);
  tc.add_game ("set", 'S', 3, 5, 4, 1);
  tc.add_game ("taboo", 'T', 6, 8, 3, 1);
  tc.add_game ("pop-5", 'F', 4, 8, 1, 1);
  tc.done ();

  tournament_t t (tc);
  int i = 0; 
  while (!t.solved ()) {
    t.schedule ();
    if (!t.solved ()) {
      warn << "Attempt #" << ++i << ": failed!\n";
      t.reset ();
    }
  }
  t.print ();
}
