#ifndef DNA_FRACTURE_TYPES_H
#define DNA_FRACTURE_TYPES_H

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DerivedMesh;

enum {
	SHARD_INTACT   = 1 << 0,
	SHARD_FRACTURED = 1 << 1,
};

typedef struct Shard {
	struct Shard *next, *prev;
	struct MVert *mvert;
	struct MPoly *mpoly;
	struct MLoop *mloop;

	struct CustomData vertData;
	struct CustomData polyData;
	struct CustomData loopData;

	int totvert, totpoly, totloop;
	int pad;
	
	int *cluster_colors;
	float min[3], max[3];
	float centroid[3];	// centroid of shard, calculated during fracture
	float start_co[3];	// hmm this was necessary for simulation itself, storing the restposition of the centroid
	int *neighbor_ids;	// neighbors of me... might be necessary for easier compounding or fracture, dont need to iterate over all.... searchradius ?
	int shard_id;	// the identifier
	int neighbor_count; // counts of neighbor islands
	int parent_id;	//the shard from which this shard originates, we keep all shards in the shardmap
	int flag;		//flag for: isValid, ..
} Shard;

typedef struct FracMesh {
	Shard **shard_map;	//groups of mesh elements to islands, hmm generated by fracture itself
	int shard_count;	//how many islands we have
	short cancel; //whether the process is cancelled (from the job, ugly, but this way we dont need the entire modifier)
	char pad[2];
} FracMesh;

typedef struct FracHistory {
	FracMesh **frac_states; // "indexed" by frames ? handle this in iterator...?
	int *frame_map; // only step in iterator when past or before according frame... important for replaying from cache
					// need a framemap; to trigger step changes by frame since we dont have neither user interaction nor feedback from the sim
					// this must be part of the history and could be in pointcache as well
	int state_count;
	char pad[4];
} FracHistory;

#ifdef __cplusplus
}
#endif

#endif // DNA_FRACTURE_TYPES_H
