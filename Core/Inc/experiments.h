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
#define EXPERIMENT_PHASE1_ENABLE (0)
#endif

/* Phase2: Priority Inversion / Mutex behavior experiment.
 *
 * Notes:
 * - Phase2 produces strict CSV output lines from a High task.
 * - For clean captures, consider disabling Phase1 logging while running Phase2
 *   to avoid concurrent UART prints interleaving.
 */
#ifndef EXPERIMENT_PHASE2_ENABLE
#define EXPERIMENT_PHASE2_ENABLE (1)
#endif

/* Compile-time mutual exclusion:
 * When Phase2 is enabled, forcibly disable Phase1 to prevent UART prints from
 * interleaving and to keep the CSV stream clean.
 */
#if (EXPERIMENT_PHASE2_ENABLE != 0)
	#undef EXPERIMENT_PHASE1_ENABLE
	#define EXPERIMENT_PHASE1_ENABLE (0)
#endif

#endif /* INC_EXPERIMENTS_H_ */

