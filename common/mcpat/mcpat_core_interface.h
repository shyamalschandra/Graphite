/*****************************************************************************
 * Graphite-McPAT Core Interface
 ***************************************************************************/

#pragma once

#include <stdio.h>
#include <string.h>
#include <iostream>
#include "fixed_types.h"
#include "instruction.h"
#include "contrib/mcpat/mcpat.h"

using namespace std;

//---------------------------------------------------------------------------
// McPAT Core Interface Data Structures for Area and Power
//---------------------------------------------------------------------------
typedef struct
{
   double area;                           // Area
   double leakage;                        // Subthreshold Leakage
   double longer_channel_leakage;         // Subthreshold Leakage
   double gate_leakage;                   // Gate Leakage
   double dynamic;                        // Runtime Dynamic
} mcpat_component_out;
typedef struct
{
   mcpat_component_out rfu;               // Register Files
   mcpat_component_out IRF;               // Integer RF
   mcpat_component_out FRF;               // Floating Point RF
   mcpat_component_out RFWIN;             // Register Windows
} mcpat_rfu_out;
typedef struct
{
   mcpat_component_out scheu;             // Instruction Scheduler
   mcpat_component_out int_inst_window;   // Instruction Window
} mcpat_scheu_out;
typedef struct
{
   mcpat_component_out ifu;               // Instruction Fetch Unit
   mcpat_component_out icache;            // Instruction Cache
   mcpat_component_out IB;                // Instruction Buffer
   mcpat_component_out ID;                // Instruction Decoder
} mcpat_ifu_out;
typedef struct
{
   mcpat_component_out lsu;               // Load Store Unit
   mcpat_component_out dcache;            // Data Cache
   mcpat_component_out LSQ;               // Load/Store Queue
} mcpat_lsu_out;
typedef struct
{
   mcpat_component_out mmu;               // Memory Management Unit
   mcpat_component_out itlb;              // Itlb
   mcpat_component_out dtlb;              // Dtlb
} mcpat_mmu_out;
typedef struct
{
   mcpat_component_out exu;               // Execution Unit
   mcpat_rfu_out rfu;                     // Register Files
   mcpat_scheu_out scheu;                 // Instruction Scheduler
   mcpat_component_out exeu;              // Integer ALUs
   mcpat_component_out fp_u;              // Floating Point Units (FPUs)
   mcpat_component_out mul;               // Complex ALUs (Mul/Div)
   mcpat_component_out bypass;            // Results Broadcast Bus
} mcpat_exu_out;
typedef struct
{
   mcpat_component_out core;              // Core
   mcpat_ifu_out ifu;                     // Instruction Fetch Unit
   mcpat_lsu_out lsu;                     // Load Store Unit
   mcpat_mmu_out mmu;                     // Memory Management Unit
   mcpat_exu_out exu;                     // Execution Unit
} mcpat_core_output;

//---------------------------------------------------------------------------
// McPAT Core Interface
//---------------------------------------------------------------------------
class McPATCoreInterface
{
public:
   enum McPATInstructionType
   {
      GENERIC_INST = 0,
      INTEGER_INST,
      FLOATING_POINT_INST,
      LOAD_INST,
      STORE_INST,
      BRANCH_INST,
      BRANCH_NOT_TAKEN_INST
   };
   enum ExecutionUnitType
   {
      ALU = 0,
      MUL,
      FPU
   };
   typedef vector<ExecutionUnitType> ExecutionUnitList;
   
   // McPAT Core Interface Constructor
   McPATCoreInterface(UInt32 technology_node, UInt32 core_frequency, UInt32 load_buffer_size, UInt32 store_buffer_size);
   // McPAT Core Interface Destructor
   ~McPATCoreInterface();

   // Update Event Counters
   void updateEventCounters(Instruction* instruction, UInt64 cycle_count);
   
   // Compute Energy from McPat
   void computeMcPATCoreEnergy();

   // Display Energy from McPat
   void displayMcPATCoreEnergy(ostream& os);
   // Display Architectural Parameters
   void displayParam(ostream& os);
   // Display Event Counters
   void displayStats(ostream& os);

private:
   // McPAT Objects
   McPAT::ParseXML  *mcpat_parsexml;
   McPAT::CoreWrapper *mcpat_core;
   // McPAT Output Data Structure
   mcpat_core_output mcpat_core_out;

   // System Parameters
   // Execution Time
   double m_execution_time;
   // |---- General Parameters
   int m_clock_rate;
   double m_core_tech_node;
   // Architectural Parameters
   // |---- General Parameters
   int m_instruction_length;
   int m_opcode_width;
   int m_machine_type;
   int m_num_hardware_threads;
   int m_fetch_width;
   int m_num_instruction_fetch_ports;
   int m_decode_width;
   int m_issue_width;
   int m_commit_width;
   int m_fp_issue_width;
   int m_prediction_width;
   int m_integer_pipeline_depth;
   int m_fp_pipeline_depth;
   int m_ALU_per_core;
   int m_MUL_per_core;
   double m_FPU_per_core;
   int m_instruction_buffer_size;
   int m_decoded_stream_buffer_size;
   // |---- Register File
   int m_arch_regs_IRF_size;
   int m_arch_regs_FRF_size;
   int m_phy_regs_IRF_size;
   int m_phy_regs_FRF_size;
   // |---- Load-Store Unit
   string m_LSU_order;
   int m_store_buffer_size;
   int m_load_buffer_size;
   int m_num_memory_ports;
   int m_RAS_size;
   // |---- OoO Core
   int m_instruction_window_scheme;
   int m_instruction_window_size;
   int m_fp_instruction_window_size;
   int m_ROB_size;
   int m_rename_scheme;
   // |---- Register Windows (specific to Sun processors)
   int m_register_windows_size;

   // Event Counters
   // |-- Used Event Counters
   // |---- Instruction Counters
   double m_total_instructions;
   double m_int_instructions;
   double m_fp_instructions;
   double m_branch_instructions;
   double m_branch_mispredictions;
   double m_load_instructions;
   double m_store_instructions;
   double m_committed_instructions;
   double m_committed_int_instructions;
   double m_committed_fp_instructions;
   // |---- Cycle Counters
   double m_total_cycles;
   double m_idle_cycles;
   double m_busy_cycles;
   // |---- Reg File Access Counters
   double m_int_regfile_reads;
   double m_int_regfile_writes;
   double m_fp_regfile_reads;
   double m_fp_regfile_writes;
   // |---- Execution Unit Access Counters
   double m_ialu_accesses;
   double m_mul_accesses;
   double m_fpu_accesses;
   double m_cdb_alu_accesses;
   double m_cdb_mul_accesses;
   double m_cdb_fpu_accesses;
   // |-- Unused Event Counters
   // |---- OoO Core Event Counters
   double m_inst_window_reads;
   double m_inst_window_writes;
   double m_inst_window_wakeup_accesses;
   double m_fp_inst_window_reads;
   double m_fp_inst_window_writes;
   double m_fp_inst_window_wakeup_accesses;
   double m_ROB_reads;
   double m_ROB_writes;
   double m_rename_accesses;
   double m_fp_rename_accesses;
   // |---- Function Calls and Context Switches
   double m_function_calls;
   double m_context_switches;
   
   bool m_enable_area_and_power_modeling;

   // Initialize Architectural Parameters
   void initializeArchitecturalParameters(UInt32 technology_node, UInt32 core_frequency, UInt32 load_buffer_size, UInt32 store_buffer_size);
   // Initialize Event Counters
   void initializeEventCounters();
   // Initialize Output Data Structure
   void initializeOutputDataStructure();

   // Update Event Counters
   void updateInstructionCounters(McPATInstructionType instruction_type);
   void updateRegFileAccessCounters(Operand::Direction operand_direction, UInt32 reg_id);
   void updateExecutionUnitAccessCounters(ExecutionUnitType unit_type);
   void updateCycleCounters(UInt64 cycle_count);

   // Initialize XML Object
   void fillCoreParamsIntoXML();
   void fillCoreStatsIntoXML();
};

McPATCoreInterface::McPATInstructionType getMcPATInstructionType(InstructionType type);
McPATCoreInterface::ExecutionUnitList getExecutionUnitAccessList(InstructionType type);
bool isIntegerReg(UInt32 reg_id);
bool isFloatingPointReg(UInt32 reg_id);
bool isXMMReg(UInt32 reg_id);
