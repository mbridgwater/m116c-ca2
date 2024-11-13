// my_predictor.h
// Hybrid predictor combining global and local history

class my_update : public branch_update {
public:
    unsigned int gindex;  // Global history index
    unsigned int lindex;  // Local history index
    unsigned int choice_index;  // Index for choice predictor
};

class my_predictor : public branch_predictor {
public:
    #define GLOBAL_HISTORY_LENGTH 16
    #define LOCAL_HISTORY_LENGTH 12
    #define GLOBAL_TABLE_BITS 14
    #define LOCAL_TABLE_BITS 14
    #define LOCAL_HISTORY_TABLE_BITS 10
    #define CHOICE_HISTORY_LENGTH 16
    #define CHOICE_TABLE_BITS 14

    my_update u;
    branch_info bi;
    
    // Global history register
    unsigned int global_history;
    
    // Local history table
    unsigned int *local_history_table;
    
    // Prediction tables
    unsigned char *global_table;
    unsigned char *local_table;
    unsigned char *choice_table;  // Choice predictor

    my_predictor(void) : global_history(0) {
        // Allocate and initialize local history table
        local_history_table = new unsigned int[1 << LOCAL_HISTORY_TABLE_BITS]();
        
        // Allocate and initialize prediction tables
        global_table = new unsigned char[1 << GLOBAL_TABLE_BITS]();
        local_table = new unsigned char[1 << LOCAL_TABLE_BITS]();
        choice_table = new unsigned char[1 << CHOICE_TABLE_BITS]();
        
        // Initialize all tables with weak taken (2)
        memset(global_table, 2, 1 << GLOBAL_TABLE_BITS);
        memset(local_table, 2, 1 << LOCAL_TABLE_BITS);
        memset(choice_table, 2, 1 << CHOICE_TABLE_BITS);
    }

    ~my_predictor(void) {
        delete[] local_history_table;
        delete[] global_table;
        delete[] local_table;
        delete[] choice_table;
    }

    // Improved hash function
    unsigned int compute_index(unsigned int address, unsigned int history, 
                             unsigned int table_bits, unsigned int history_length) {
        unsigned int index = address ^ (address >> (table_bits/2));
        index ^= history ^ (history << (table_bits/3));
        index ^= (history >> (history_length/2)) * 7919;  // Prime multiplier
        return index & ((1 << table_bits) - 1);
    }

    branch_update *predict(branch_info & b) {
        bi = b;
        if (b.br_flags & BR_CONDITIONAL) {
            unsigned int local_hist_idx = (b.address >> 2) & ((1 << LOCAL_HISTORY_TABLE_BITS) - 1);
            unsigned int local_hist = local_history_table[local_hist_idx];

            // Compute indices for both predictors and choice
            u.gindex = compute_index(b.address, global_history, 
                                   GLOBAL_TABLE_BITS, GLOBAL_HISTORY_LENGTH);
            u.lindex = compute_index(b.address, local_hist,
                                   LOCAL_TABLE_BITS, LOCAL_HISTORY_LENGTH);
            u.choice_index = compute_index(b.address, global_history,
                                         CHOICE_TABLE_BITS, CHOICE_HISTORY_LENGTH);

            // Get predictions
            bool global_pred = global_table[u.gindex] >> 1;
            bool local_pred = local_table[u.lindex] >> 1;
            bool use_global = choice_table[u.choice_index] >> 1;

            // Final prediction based on choice predictor
            u.direction_prediction(use_global ? global_pred : local_pred);
        } else {
            u.direction_prediction(true);
        }
        u.target_prediction(0);
        return &u;
    }

    void update(branch_update *u, bool taken, unsigned int target) {
        if (bi.br_flags & BR_CONDITIONAL) {
            my_update *mu = (my_update*)u;
            unsigned int local_hist_idx = (bi.address >> 2) & ((1 << LOCAL_HISTORY_TABLE_BITS) - 1);

            // Update predictors
            bool global_pred = global_table[mu->gindex] >> 1;
            bool local_pred = local_table[mu->lindex] >> 1;

            // Update choice predictor
            if (global_pred != local_pred) {
                unsigned char *choice_counter = &choice_table[mu->choice_index];
                if (global_pred == taken) {
                    if (*choice_counter < 3) (*choice_counter)++;
                } else {
                    if (*choice_counter > 0) (*choice_counter)--;
                }
            }

            // Update global table
            unsigned char *global_counter = &global_table[mu->gindex];
            if (taken) {
                if (*global_counter < 3) (*global_counter)++;
            } else {
                if (*global_counter > 0) (*global_counter)--;
            }

            // Update local table
            unsigned char *local_counter = &local_table[mu->lindex];
            if (taken) {
                if (*local_counter < 3) (*local_counter)++;
            } else {
                if (*local_counter > 0) (*local_counter)--;
            }

            // Update histories
            local_history_table[local_hist_idx] = ((local_history_table[local_hist_idx] << 1) | taken) 
                                                 & ((1 << LOCAL_HISTORY_LENGTH) - 1);
            global_history = ((global_history << 1) | taken) & ((1 << GLOBAL_HISTORY_LENGTH) - 1);
        }
    }
};