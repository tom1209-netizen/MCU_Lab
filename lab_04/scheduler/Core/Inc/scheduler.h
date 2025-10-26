#ifndef INC_SCHEDULER_H_
#define INC_SCHEDULER_H_

#include "stdint.h"

// --- Define constants ---

// MUST BE ADJUSTED FOR EACH NEW PROJECT
#define SCH_MAX_TASKS 40
// TaskID 0 is reserved for "no task" or "error"
#define NO_TASK_ID 0 

// Error code definitions
#define ERROR_SCH_TOO_MANY_TASKS (1)
#define ERROR_SCH_CANNOT_DELETE_TASK (2) // Not used in heap, but kept for compatibility
#define ERROR_SCH_HEAP_FULL (3)

// Return code definitions
#define RETURN_NORMAL (0)
#define RETURN_ERROR (1)


// --- Task Structure ---

// NOTE: This struct has changed
typedef struct {
    // Pointer to the task
    void (*pTask)(void);
    // Interval (ticks) between subsequent runs (0 for one-shot)
    uint32_t Period;
    // Absolute tick count when the task is due to run
    uint32_t RunAtTick;
    // A unique ID for this task
    uint32_t TaskID;
    
} sTask;


// --- Global Tick Access ---

/**
 * @brief Returns the current scheduler tick count.
 */
uint32_t SCH_Get_Current_Tick(void);


// --- Function Prototypes ---

/**
 * @brief Initializes the scheduler heap and timer.
 */
void SCH_Init(void);

/**
 * @brief The scheduler update function (ISR).
 * This is now an O(1) operation.
 */
void SCH_Update(void);

/**
 * @brief Adds a task to the scheduler heap.
 * This is now an O(log n) operation.
 * @param pFunction Pointer to the task function.
 * @param DELAY The delay (in ticks) before the task is first executed.
 * @param PERIOD The interval (in ticks) for periodic tasks (0 for one-shot).
 * @return A unique TaskID (non-zero). Returns 0 (NO_TASK_ID) if scheduler is full.
 */
uint32_t SCH_Add_Task(void (*pFunction)(), uint32_t DELAY, uint32_t PERIOD);

/**
 * @brief The scheduler dispatcher.
 * This is now O(log n) *per task that runs*.
 */
void SCH_Dispatch_Tasks(void);

/**
 * @brief Deletes a task from the scheduler heap.
 * This is an O(n) operation (to find the task)
 * followed by O(log n) (to fix the heap).
 * @param taskID The ID of the task to delete.
 * @return RETURN_NORMAL or RETURN_ERROR.
 */
unsigned char SCH_Delete_Task(const uint32_t taskID);

/**
 * @brief Optional function to report errors.
 */
void SCH_Report_Status(void);

/**
 * @brief Optional function to put the MCU to sleep.
 */
void SCH_Go_To_Sleep(void);

#endif /* INC_SCHEDULER_H_ */