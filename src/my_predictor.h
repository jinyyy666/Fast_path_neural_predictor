// my_predictor.h
// This file contains a sample my_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.
// Note that this predictor doesn't use the whole 8 kilobytes available
// for the CBP-2 contest; it is just an example.

class my_update : public branch_update {
public:
	unsigned int index;
};

class my_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	7
#define WEIGHT_LENGTH 8
#define UNSIGNED 1
#define WEIGHT_TABLE_LENGTH 1024
	my_update u;
	branch_info bi;
	unsigned int history;     // global history
	unsigned int i;           // the row index of the hashed pc to weight matrix
	unsigned int h;           // the history length;
	int y;                    // output of the perpectron
	int r[HISTORY_LENGTH+1];  // shift register recording the non-speculate partial sums, additional one bit is for the bias
	int v[HISTORY_LENGTH];    // the index of the row in W used for predicting the j + 1 most recent branch, note that j = 0 ... h - 1 !!!
	int weights[WEIGHT_TABLE_LENGTH][HISTORY_LENGTH+1]; // the weight matrix
	const int thresh;

	my_predictor (void) : history(0), h(HISTORY_LENGTH), thresh(int(2.14*(HISTORY_LENGTH) + 20.58)){ 
		memset(weights, 0, sizeof(weights));
		memset(r, 0, sizeof(r));
		memset(v, 0, sizeof(v));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if(b.br_flags & BR_CONDITIONAL){
		    // Hash pc to produce the row number in W and calculate the output:
		    u.index = history ^ (b.address & ((1<<h) -1));
		    i = b.address % WEIGHT_TABLE_LENGTH;
		    y = r[h] + weights[i][0];
		    if(y >= 0)
		        u.direction_prediction(true);
		    else
		        u.direction_prediction(false);
		}
		else{
		    u.direction_prediction(true);
		}

		u.target_prediction (0);
		return &u;

	}

	void update (branch_update *u, bool taken, unsigned int target) {
	        h = HISTORY_LENGTH;
		if (bi.br_flags & BR_CONDITIONAL) {
		    // Update the next h non-speculate partial sums:
		    for(unsigned int j = 1; j <= h; ++j){
		        unsigned int k = h - j;
			if(taken)
			    r[k+1] = r[k] + weights[i][j];
			else
			    r[k+1] = r[k] - weights[i][j];
		    }
		    r[0] = 0;

		    // Update the weight matrix:
		    if(u->direction_prediction() != taken || abs(y) <= thresh){
			update_weight(i, 0, taken, true);
			//weights[i][0] += taken == true ? 1 : -1;
			for(unsigned int j = 1; j <= h; ++j){
			    unsigned int k = v[j-1];
			    // Use the history information to update the weights:
			    update_weight(k, j, taken, 1&(history>>(j-1)));
			}
		    }
		    
		    // Update the row number v[j] (j = 1 .. h) of weight matrix for next j branches 
		    // Please note the the v[j] is of size h !!!! not h+1
		    for(unsigned int j = 1; j < h; ++j){
		        unsigned int k = h - j;
		        v[k] = v[k-1];
		    }
		    // Update v[0]:
		    v[0] = i;
		    
		    // Update the global history
		    history <<= 1;
		    history |= taken;
		    history &= (1<<HISTORY_LENGTH)-1;
		    
		    // No need to restore the speculative history on a misprediction 
		    //  because the update happens right after the prediction
		}
	}
	// the function for weight update, need to consider the range of weights!!!
	// the weight should be within  -2^{WEIGHT_LENGTH-1} + 1 to 2^{WEIGHT_LENGTH-1}
	void update_weight(int i, int j, bool taken , bool target){
		if(taken == target){
#if	UNSIGNED
			if(weights[i][j] < (1<<(WEIGHT_LENGTH)))
#else
			if(weights[i][j] < (1<<(WEIGHT_LENGTH-1)))
#endif
				weights[i][j] += 1;
		}
		else{
#if	UNSIGNED
			if(weights[i][j] > 0)
#else
			if(weights[i][j] > (1 - 1*(1<<(WEIGHT_LENGTH-1))))
#endif
				weights[i][j] -= 1;
		}
	}
};
