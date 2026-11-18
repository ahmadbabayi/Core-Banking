#ifndef TRANSFERSTATETRANSITIONS_H
#define TRANSFERSTATETRANSITIONS_H

namespace TransferStateTransitions {

// INITIATED -> PROCESSING. The status guard makes the update fail closed when
// the transaction row is not in the expected starting state. The table name is
// quoted so the same statement text is valid for both PostgreSQL and the
// focused SQLite transition test.
inline constexpr const char* kInitiatedToProcessing =
    "UPDATE \"transaction\" SET status = 'PROCESSING', "
    "    updated_at = CURRENT_TIMESTAMP "
    "WHERE id = :id AND status = 'INITIATED'";

// PROCESSING -> COMPLETED. The PROCESSING guard makes COMPLETED unreachable
// without passing through the intermediate state.
inline constexpr const char* kProcessingToCompleted =
    "UPDATE \"transaction\" SET status = 'COMPLETED', result_code = 'TRANSFER_COMPLETED', "
    "    result_message = 'Transfer completed', completed_at = CURRENT_TIMESTAMP, "
    "    updated_at = CURRENT_TIMESTAMP "
    "WHERE id = :id AND status = 'PROCESSING'";

}

#endif
