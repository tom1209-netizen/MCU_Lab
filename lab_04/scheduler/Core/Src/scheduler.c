#include "scheduler.h"
#include "main.h"

// --- Global Variables ---

// The array of tasks, used as storage for the min-heap
sTask SCH_tasks_G[SCH_MAX_TASKS];

// Global error variable
unsigned char Error_code_G = 0;

// Variables for error reporting
static unsigned char Last_error_code_G = 0;
static uint32_t Error_tick_count_G = 0;

// --- Heap-specific Variables ---

// The scheduler's master time, incremented by the ISR
static volatile uint32_t g_CurrentTick = 0;
// Current number of tasks in the heap
static uint32_t g_HeapSize = 0;
// Used to assign unique IDs to tasks
static uint32_t g_NextTaskID = 1; // 0 is reserved for NO_TASK_ID

// --- Private Function Prototypes (Heap Utilities) ---

extern void Timer_init(void);
static void SwapTasks(uint32_t index1, uint32_t index2);
static uint8_t isTaskHigherPriority(uint32_t index1, uint32_t index2);
static void HeapifyUp(uint32_t index);
static void HeapifyDown(uint32_t index);
static int32_t FindTaskIndex(uint32_t taskID);

// --- Public Function Implementations ---

/**
 * @brief Returns the current scheduler tick count.
 */
uint32_t SCH_Get_Current_Tick(void) { return g_CurrentTick; }

/**
 * @brief SCH_Init implementation (O(1))
 */
void SCH_Init(void) {
  g_HeapSize = 0;
  g_CurrentTick = 0;
  g_NextTaskID = 1; // Start IDs from 1
  Error_code_G = 0;

  // Initialize the timer
  Timer_init();
}

/**
 * @brief SCH_Update implementation (ISR, O(1))
 * This function is now extremely fast.
 */
void SCH_Update(void) {
  // Just increment the master tick.
  // The dispatcher will do all the work.
  g_CurrentTick++;
}

/**
 * @brief SCH_Add_Task implementation (O(log n))
 */
uint32_t SCH_Add_Task(void (*pFunction)(), uint32_t DELAY, uint32_t PERIOD) {
  // Check if the heap is full
  if (g_HeapSize >= SCH_MAX_TASKS) {
    Error_code_G = ERROR_SCH_HEAP_FULL;
    return NO_TASK_ID;
  }

  // Get a new Task ID
  uint32_t newTaskID = g_NextTaskID++;

  // Calculate the absolute time to run
  // Note: We read the volatile tick *once*
  uint32_t runAt = g_CurrentTick + DELAY;

  // Get the index for the new task (end of the heap)
  uint32_t newIndex = g_HeapSize;

  // Add the new task to the end of the heap
  SCH_tasks_G[newIndex].pTask = pFunction;
  SCH_tasks_G[newIndex].Period = PERIOD;
  SCH_tasks_G[newIndex].RunAtTick = runAt;
  SCH_tasks_G[newIndex].TaskID = newTaskID;

  // Increment the heap size
  g_HeapSize++;

  // Fix the heap by bubbling the new task up
  HeapifyUp(newIndex);

  // Return the new task's ID
  return newTaskID;
}

/**
 * @brief SCH_Dispatch_Tasks implementation
 */
void SCH_Dispatch_Tasks(void) {
  // Loop while there are tasks and the top task is due
  // g_CurrentTick is volatile, so it's re-read each loop
  while (g_HeapSize > 0 && SCH_tasks_G[0].RunAtTick <= g_CurrentTick) {
    // Extract the Root Task
    sTask taskToRun = SCH_tasks_G[0];
    // Move the last task to the root
    SCH_tasks_G[0] = SCH_tasks_G[g_HeapSize - 1];
    g_HeapSize--;
    // Fix the heap by bubbling the new root down
    if (g_HeapSize > 0) {
      HeapifyDown(0);
    }

    // Run the Task
    (*taskToRun.pTask)();

    // Re-insert if Periodic
    if (taskToRun.Period > 0) {
      // Check if heap has space before re-inserting
      if (g_HeapSize < SCH_MAX_TASKS) {
        // Calculate new runtime and add back to heap
        // This prevents task "drift"
        taskToRun.RunAtTick += taskToRun.Period;

        // Add it back as a "new" task
        uint32_t newIndex = g_HeapSize;
        SCH_tasks_G[newIndex] = taskToRun;
        g_HeapSize++;
        HeapifyUp(newIndex);
      } else {
        // Heap is full, can't re-schedule. Set error.
        Error_code_G = ERROR_SCH_HEAP_FULL;
      }
    }
    // If Period is 0 (one-shot), we just don't re-insert it.
  }

  // Report system status
  SCH_Report_Status();

  // The scheduler enters idle mode at this point
  SCH_Go_To_Sleep();
}

/**
 * @brief SCH_Delete_Task implementation (O(n))
 */
unsigned char SCH_Delete_Task(const uint32_t taskID) {
  if (taskID == NO_TASK_ID) {
    return RETURN_ERROR;
  }

  // Find the task's current index in the heap (O(n))
  int32_t index = FindTaskIndex(taskID);

  if (index == -1) {
    // Task not found
    Error_code_G = ERROR_SCH_CANNOT_DELETE_TASK;
    return RETURN_ERROR;
  }

  // Perform heap deletion (O(log n))
  // Swap the task to be deleted with the last task
  SwapTasks(index, g_HeapSize - 1);

  // Remove the last task (which is the one we want to delete)
  g_HeapSize--;

  // Fix the heap
  // The task we just swapped into 'index' might be in the wrong place.
  // We try to bubble it both up and down (only one will actually do anything).
  if (index < g_HeapSize) // Check if it wasn't the last element
  {
    HeapifyUp(index);
    HeapifyDown(index);
  }

  return RETURN_NORMAL;
}

/**
 * @brief SCH_Report_Status implementation
 */
void SCH_Report_Status(void) {
#ifdef SCH_REPORT_ERRORS
  if (Error_code_G != Last_error_code_G) {
    Last_error_code_G = Error_code_G;

    if (Error_code_G != 0) {
      Error_tick_count_G = 60000;
    } else {
      Error_tick_count_G = 0;
    }
  } else {
    if (Error_tick_count_G != 0) {
      if (--Error_tick_count_G == 0) {
        Error_code_G = 0; // Reset error code
      }
    }
  }
#endif
}

/**
 * @brief SCH_Go_To_Sleep implementation
 */
void SCH_Go_To_Sleep() {}

// --- Private Heap Utility Functions ---

/**
 * @brief Swaps two tasks in the global heap array
 */
static void SwapTasks(uint32_t index1, uint32_t index2) {
  sTask temp = SCH_tasks_G[index1];
  SCH_tasks_G[index1] = SCH_tasks_G[index2];
  SCH_tasks_G[index2] = temp;
}

/**
 * @brief Compares two tasks in the heap.
 * @return 1 if task at index1 has higher priority (should run sooner), 0
 * otherwise.
 */
static uint8_t isTaskHigherPriority(uint32_t index1, uint32_t index2) {
  if (SCH_tasks_G[index1].RunAtTick < SCH_tasks_G[index2].RunAtTick) {
    // Task 1 has an earlier runtime
    return 1;
  } else if (SCH_tasks_G[index1].RunAtTick > SCH_tasks_G[index2].RunAtTick) {
    // Task 1 has a later runtime
    return 0;
  } else {
    // Runtimes are equal, use TaskID as a tie-breaker (FIFO)
    // The task added first (lower ID) has higher priority.
    return (SCH_tasks_G[index1].TaskID < SCH_tasks_G[index2].TaskID);
  }
}

/**
 * @brief Bubbles a task *up* the heap (O(log n))
 */
static void HeapifyUp(uint32_t index) {
  if (index == 0)
    return;

  uint32_t parent = (index - 1) / 2;

  // While this node is higher priority than its parent, swap them
  while (index > 0 && isTaskHigherPriority(index, parent)) {
    SwapTasks(index, parent);
    index = parent;
    parent = (index - 1) / 2;
  }
}

/**
 * @brief Bubbles a task *down* the heap (O(log n))
 */
static void HeapifyDown(uint32_t index) {
  uint32_t smallestOrHighestPriority = index;
  uint32_t leftChild = (2 * index) + 1;
  uint32_t rightChild = (2 * index) + 2;

  // Find the highest priority of the node and its two children
  if (leftChild < g_HeapSize &&
      isTaskHigherPriority(leftChild, smallestOrHighestPriority)) {
    smallestOrHighestPriority = leftChild;
  }

  if (rightChild < g_HeapSize &&
      isTaskHigherPriority(rightChild, smallestOrHighestPriority)) {
    smallestOrHighestPriority = rightChild;
  }

  // If the highest priority is not the root, swap and recurse
  if (smallestOrHighestPriority != index) {
    SwapTasks(index, smallestOrHighestPriority);
    HeapifyDown(smallestOrHighestPriority);
  }
}

/**
 * @brief Finds the array index of a task by its ID (O(n))
 * @return Index of the task, or -1 if not found.
 */
static int32_t FindTaskIndex(uint32_t taskID) {
  for (uint32_t i = 0; i < g_HeapSize; i++) {
    if (SCH_tasks_G[i].TaskID == taskID) {
      return (int32_t)i;
    }
  }
  return -1; // Not found
}