#pragma once

#define SEED 15210
#define K_CAP 4

#include <random>
#include <iostream>
#include "parlay/primitives.h"
#include "parlay/random.h"
#include "helper/ligra_light.h"

using namespace parlay;

#define CAS_FAIL 0
#define CAS_SUCCEED 1
#define CAS_RETRY 2

size_t log2floor(size_t i){
	int count = 0;
	while(i > 1){
		i/=2;
		count++;
	}
	return count;
}

class le_list{
	public:
		sequence<sequence<std::pair<size_t,int>>> A;
		sequence<std::atomic_size_t> sizes;
		size_t capacity;

		le_list(size_t n){
			capacity = K_CAP * (log2floor(n) + 1);
			size_t cap = capacity;
			A = tabulate(n, [cap](size_t i){
				return tabulate(cap, [](size_t j){
					return std::make_pair((size_t) -1, INT_MAX);
				});
			});


			sizes = sequence<std::atomic_size_t>(n);
			parallel_for(0, n, [&](size_t i){
				sizes[i] = 0;
			});
		}

		//Inserts (j,dist) into i's LE-List
		void insert(int i, int j, int dist){
			size_t ind = sizes[i]++;
			if(ind >= capacity){
				fprintf(stderr,"Error: Exceeded list capacity. Try again with larger K_CAP value.\n");
				fprintf(stderr,"List Contents: (Vertex %d) [",i);
				for(size_t k=0;k<ind;k++){
					fprintf(stderr,"(%lu,%d),",A[i][k].first,A[i][k].second);
				}
				fprintf(stderr,"]\n");
				exit(-1);
			}
			A[i][ind] = std::make_pair(j,dist);
		}

		//Returns a sequence representation of le-lists. The order of the lists is unspecified
		sequence<sequence<std::pair<size_t,int>>> pack(){
			return tabulate(sizes.size(), [this](size_t i){
				return to_sequence(A[i].head(sizes[i]));
			});
		}
};


bool compare_and_swap_less(std::atomic<int>& loc, int val){
	int old = loc;
	return (val >= old) || loc.compare_exchange_strong(old, val);
}

int compare_and_swap_less(std::atomic<int>& loc, int val, sequence<int> order){
	int old = loc;
	if(old == INT_MAX || (order[val] < order[old])){
		if(loc.compare_exchange_strong(old,val)){
			return CAS_SUCCEED;
		}
		else{
			return CAS_RETRY;
		}
	}
	return CAS_FAIL;
}

bool compare_and_swap_greater(std::atomic<int>& loc, int val){
	int old = loc;
	return (val > old) && loc.compare_exchange_strong(old, val);
}

int fetch_and_min(std::atomic<int>& loc, int val, sequence<int> order){
	int result = compare_and_swap_less(loc,val,order);
	while(result == CAS_RETRY){
		result = compare_and_swap_less(loc,val,order);
	}
	return result;
}

struct vertex_info{
	std::atomic<int> root;
	std::atomic<int> root_ro;
	std::atomic<int> step;

//	vertex_info(int _root, int _root_ro, int _step):root(_root),root_ro(_root_ro),step(_step){}
//		std::atomic<int> root(_root);
//		std::atomic<int> root_ro(_root_ro);
//		std::atomic<int> step(_step);
//	}
};

template <typename vertex, typename graph>
auto TruncatedBFS(graph& G, graph& GT, sequence<vertex>& srcs,
	              sequence<std::atomic<int>>& delta_ro, sequence<std::atomic<int>>& delta, le_list& L){
	int n = G.size();
	auto order = tabulate(n, [&](size_t i) { return INT_MAX; });
//	auto vtxs = tabulate<struct vertex_info>(n, [&](size_t i){
//		return vertex_info(-1, -1, (int) 0);
//	});
//	auto vtxs = sequence<struct vertex_info>::from_function(n, [&](size_t i){
//		return vertex_info(-1, -1, (int) 0);
//	});
	auto vtxs = sequence<struct vertex_info>(n);
	parallel_for(0, n, [&](size_t i){
		vtxs[i].root = INT_MAX;
		vtxs[i].root_ro = INT_MAX;
		vtxs[i].step = 0;
	});
	size_t dist = 0;

	parallel_for(0,parlay::size(srcs),[&](size_t i){
		delta[srcs[i]] = 0;
		order[srcs[i]] = i;
		vtxs[srcs[i]].root = srcs[i];
		vtxs[srcs[i]].root_ro = srcs[i];
		vtxs[srcs[i]].step = 0;
		L.insert(srcs[i],srcs[i],0);
	});

	auto edge_f = [&] (vertex s, vertex d) -> bool {
//		std::cout << "Edge f: " << s << "," << vtxs[s].root << "," << vtxs[s].root_ro << std::endl;
		if((vtxs[d].root_ro == INT_MAX || order[vtxs[s].root_ro] < order[vtxs[d].root_ro])
		   && (dist < delta_ro[d])){
			if(fetch_and_min(vtxs[d].root, vtxs[s].root_ro, order)){//Passed by value?
				L.insert(d,vtxs[s].root_ro,dist);
				while(!compare_and_swap_less(delta[d], dist));
				return compare_and_swap_greater(vtxs[d].step, dist);
			}
			return false;
		}
		return false;
	};

	auto cond_f = [&] (vertex d) {
		int tmp = vtxs[d].root;
		return dist < delta_ro[d] && (tmp == INT_MAX || order[tmp] > 0);
	};

	auto frontier_map = ligra::edge_map(G, GT, edge_f, cond_f);
//	vertexSubset Frontier(G.n, std::move(srcs));
	auto frontier = ligra::vertex_subset(srcs);

	while (frontier.size() > 0) {
		dist++;

//		auto bfs_f = Trunc_BFS_F<W>(dist,vtxs,order.begin(),L,delta_ro,delta);

		frontier = frontier_map(frontier);
//		Frontier = edgeMap(G, Frontier, bfs_f ,
//		                   -1, sparse_blocked | dense_parallel);

//		for(int v=0; v< frontier.size(); v++){
		auto frontier2 = frontier.to_seq();
		parallel_for(0, frontier2.size(), [&](size_t v){
//			std::cout << "Copy: " << v << "," << vtxs[v].root << "," << vtxs[v].root.load() << std::endl;
			vtxs[frontier2[v]].root_ro = vtxs[frontier2[v]].root.load();
		});
//		}
//		vertexMap(Frontier, [&](int v){ vtxs[v].root_ro = vtxs[v].root; });
	}
}

template <class Graph>
inline auto create_le_list(Graph& G, Graph& GT) {
//	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	unsigned seed = SEED;
	std::default_random_engine generator(seed);
	std::uniform_real_distribution<double> distribution(0.0,1.0);

	int n = G.size();

	//Permute Vertices
	auto R = tabulate(n, [&](int i){ return distribution(generator); });

	auto verts = tabulate(n, [&](int i){ return i;});
	auto P = stable_sort(verts, [&R](int u, int v){
		return R[u] < R[v];
	});

	auto PInv = sequence<int>(n);
	parallel_for(0, n, [&](size_t i){
		PInv[P[i]] = i;
	});

	le_list L(n);

	auto delta = tabulate<std::atomic<int>>(n, [](int i){return INT_MAX;});

	//Build LE-Lists - Parallel
	//Handle r=0 case here
	auto P2 = tabulate(n, [&](size_t i){return (int) P[i];});

	auto delta_ro = tabulate<std::atomic<int>>(n, [&](int i){return delta[i].load();});
	auto srcs = tabulate(1,[&](int i){ return P2[0]; });
	TruncatedBFS(G,GT,srcs,delta_ro,delta,L);
	for(int r=1;r<2*n;r*=2){
		if(r >= n){
			continue;
		}
		auto delta_ro = tabulate<std::atomic<int>>(n, [&delta](int i){return delta[i].load();});
		srcs = to_sequence(P2.cut(r,std::min(2*r,n)));
		TruncatedBFS(G,GT,srcs,delta_ro,delta,L);
	}
	sequence<sequence<std::pair<size_t,int>>> lists = L.pack();

	parallel_for(0, n, [&](size_t i){
		//Sort lists in order of increasing vertex number (based on permutation)
		//Also sorts in order of decreasing distance
		sort_inplace(lists[i], [&PInv](std::pair<size_t,int> p1, std::pair<size_t,int> p2){
			return PInv[p1.first] < PInv[p2.first];
		});

		//Prune lists for "extra elements"
		auto flags = tabulate(parlay::size(lists[i]), [&lists, i](int j){
			return j==0 || lists[i][j].second < lists[i][j-1].second;
		});

		lists[i] = pack(lists[i],flags);
	});

	return lists;
}
