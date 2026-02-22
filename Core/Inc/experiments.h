#ifndef INC_EXPERIMENTS_H_
#define INC_EXPERIMENTS_H_

/*
 * Experiment feature switches
 *
 * - You can override these from the compiler command line, e.g.
 *   -DEXPERIMENT_PHASE1_ENABLE=0
 *
 * - Default keeps Phase1 enabled to preserve current behavior.
 */

#ifndef EXPERIMENT_PHASE1_ENABLE
#define EXPERIMENT_PHASE1_ENABLE (1)
#endif

#endif /* INC_EXPERIMENTS_H_ */
