
#include <stdio.h>
#include <string.h>

#define MAX_ZONES        6
#define ID_LEN           8
#define NAME_LEN         40
#define TYPE_LEN         20
#define COND_LEN         30

#define WEIGHT_HOSPITAL           30.0
#define WEIGHT_EMERGENCY_SERVICE  30.0
#define WEIGHT_EMERGENCY_SHELTER  20.0
#define WEIGHT_RESIDENTIAL        10.0


#define WAITING_CYCLE_WEIGHT       5.0
#define URGENCY_WEIGHT            15.0
#define LOSS_PENALTY_WEIGHT        0.3

typedef struct {
    char   zoneID[ID_LEN];
    char   zoneName[NAME_LEN];
    char   type[TYPE_LEN];
    double requested;
    double minRequirement;
    double lossPercent;
    int    waitingCycles;

    double priorityScore;
    int    priorityRank;
    double allocated;
    double lossAmount;
    double effectiveDelivered;
    double shortage;
    char   condition[COND_LEN];
} Zone;


void   initializeZones(Zone zones[], int n);
double typeWeight(const char *type);
void   computePriorityScores(Zone zones[], int n);
void   sortByPriority(Zone zones[], int n);
void   allocateWater(Zone zones[], int n, double availableWater);
void   determineServiceConditions(Zone zones[], int n);
int    linearSearchByID(const Zone zones[], int n, const char *id);
void   sortByShortageDesc(Zone zones[], int n, Zone sorted[]);
int    findHighestUnresolved(const Zone zones[], int n);
void   printAllocationSummary(const Zone zones[], int n, double availableWater);
void   printTotalsAndCounts(const Zone zones[], int n, double availableWater);
void   editZoneData(Zone zones[], int n);

int main(void) {
    Zone zones[MAX_ZONES];
    double availableWater;
    int choice;

    initializeZones(zones, MAX_ZONES);

    printf("============================================================\n");
    printf(" EMERGENCY POTABLE WATER ALLOCATION SYSTEM\n");
    printf("============================================================\n");

    do {
        printf("\nCurrent total water available at distribution centre: ");
        printf("(default is 13500 L if you enter 0)\n");
        printf("Enter available water (litres): ");
        if (scanf("%lf", &availableWater) != 1) {
            printf("Invalid input.\n");
            return 1;
        }
        if (availableWater <= 0) availableWater = 13500.0;

        printf("\nDo you want to edit any zone's data before allocation?\n");
        printf(" 1. No, proceed with current data\n");
        printf(" 2. Yes, edit a zone (change loss %% or waiting cycles)\n");
        printf("Choice: ");
        scanf("%d", &choice);
        if (choice == 2) {
            editZoneData(zones, MAX_ZONES);
        }

        computePriorityScores(zones, MAX_ZONES);
        sortByPriority(zones, MAX_ZONES);
        allocateWater(zones, MAX_ZONES, availableWater);
        determineServiceConditions(zones, MAX_ZONES);

        printAllocationSummary(zones, MAX_ZONES, availableWater);
        printTotalsAndCounts(zones, MAX_ZONES, availableWater);

        /* --- searching demonstration -------------------------------- */
        char searchID[ID_LEN];
        printf("\nEnter a Zone ID to look up (e.g. Z03), or 'x' to skip: ");
        scanf("%7s", searchID);
        if (strcmp(searchID, "x") != 0 && strcmp(searchID, "X") != 0) {
            int idx = linearSearchByID(zones, MAX_ZONES, searchID);
            if (idx >= 0) {
                printf("\nFound: %s (%s, %s)\n", zones[idx].zoneID,
                       zones[idx].zoneName, zones[idx].type);
                printf("  Requested: %.1f L | Allocated: %.1f L | "
                       "Effective: %.1f L | Shortage: %.1f L | %s\n",
                       zones[idx].requested, zones[idx].allocated,
                       zones[idx].effectiveDelivered, zones[idx].shortage,
                       zones[idx].condition);
            } else {
                printf("Zone ID not found.\n");
            }
        }

        /* --- ordering demonstration (by remaining shortage) ----------- */
        Zone shortageOrder[MAX_ZONES];
        sortByShortageDesc(zones, MAX_ZONES, shortageOrder);
        printf("\nZones ordered by remaining shortage (highest first):\n");
        for (int i = 0; i < MAX_ZONES; i++) {
            printf("  %d. %-6s %-22s Shortage: %.1f L\n", i + 1,
                   shortageOrder[i].zoneID, shortageOrder[i].zoneName,
                   shortageOrder[i].shortage);
        }

        int worst = findHighestUnresolved(zones, MAX_ZONES);
        printf("\nZone with the highest unresolved requirement: %s (%s), "
               "shortage = %.1f L\n",
               zones[worst].zoneID, zones[worst].zoneName,
               zones[worst].shortage);

        printf("\nRun another scenario with different input values? (1=Yes, 0=No): ");
        scanf("%d", &choice);
    } while (choice == 1);

    printf("\nProgramme terminated.\n");
    return 0;
}

void initializeZones(Zone zones[], int n) {
    Zone base[MAX_ZONES] = {
        {"Z01", "Municipal Hospital",      "Hospital",          4000, 3000, 2,  0, 0,0,0,0,0,0,""},
        {"Z02", "Central Flood Shelter",   "Emergency Shelter", 3500, 2500, 4,  1, 0,0,0,0,0,0,""},
        {"Z03", "Ward 3 Residential Area", "Residential",       4500, 2000, 8,  2, 0,0,0,0,0,0,""},
        {"Z04", "Ward 5 Residential Area", "Residential",       3800, 1800, 12, 3, 0,0,0,0,0,0,""},
        {"Z05", "School Emergency Shelter","Emergency Shelter", 2600, 1600, 5,  1, 0,0,0,0,0,0,""},
        {"Z06", "Fire and Emergency Service","Emergency Service",2000,1500, 1,  0, 0,0,0,0,0,0,""}
    };
    for (int i = 0; i < n; i++) {
        zones[i] = base[i];
    }
}

double typeWeight(const char *type) {
    if (strcmp(type, "Hospital") == 0)            return WEIGHT_HOSPITAL;
    if (strcmp(type, "Emergency Service") == 0)   return WEIGHT_EMERGENCY_SERVICE;
    if (strcmp(type, "Emergency Shelter") == 0)   return WEIGHT_EMERGENCY_SHELTER;
    if (strcmp(type, "Residential") == 0)         return WEIGHT_RESIDENTIAL;
    return 0.0;
}

void computePriorityScores(Zone zones[], int n) {
    for (int i = 0; i < n; i++) {
        double urgencyRatio = (zones[i].requested > 0)
                                ? (zones[i].minRequirement / zones[i].requested)
                                : 0.0;
        zones[i].priorityScore =
              typeWeight(zones[i].type)
            + (zones[i].waitingCycles * WAITING_CYCLE_WEIGHT)
            + (urgencyRatio * URGENCY_WEIGHT)
            - (zones[i].lossPercent * LOSS_PENALTY_WEIGHT);
    }
}

static int higherPriority(const Zone *a, const Zone *b) {
    if (a->priorityScore != b->priorityScore)
        return a->priorityScore > b->priorityScore;
    if (a->waitingCycles != b->waitingCycles)
        return a->waitingCycles > b->waitingCycles;
    if (typeWeight(a->type) != typeWeight(b->type))
        return typeWeight(a->type) > typeWeight(b->type);
    return strcmp(a->zoneID, b->zoneID) < 0;
}

void sortByPriority(Zone zones[], int n) {
    for (int i = 1; i < n; i++) {
        Zone key = zones[i];
        int j = i - 1;
        while (j >= 0 && higherPriority(&key, &zones[j])) {
            zones[j + 1] = zones[j];
            j--;
        }
        zones[j + 1] = key;
    }
    for (int i = 0; i < n; i++) {
        zones[i].priorityRank = i + 1;
    }
}


void allocateWater(Zone zones[], int n, double availableWater) {
    double remaining = availableWater;

    for (int i = 0; i < n; i++) {
        zones[i].allocated = 0;
    }

    for (int i = 0; i < n && remaining > 0; i++) {
        double lossFraction = zones[i].lossPercent / 100.0;

        double releaseForMin = (lossFraction < 1.0)
                ? zones[i].minRequirement / (1.0 - lossFraction)
                : zones[i].minRequirement;

        double give = (releaseForMin <= remaining) ? releaseForMin : remaining;
        zones[i].allocated += give;
        remaining -= give;
    }

    for (int i = 0; i < n && remaining > 0; i++) {
        double alreadyReleased = zones[i].allocated;
        double stillWanted = zones[i].requested - alreadyReleased;
        if (stillWanted <= 0) continue;

        double give = (stillWanted <= remaining) ? stillWanted : remaining;
        zones[i].allocated += give;
        remaining -= give;
    }
    for (int i = 0; i < n; i++) {
        double lossFraction = zones[i].lossPercent / 100.0;
        zones[i].lossAmount = zones[i].allocated * lossFraction;
        zones[i].effectiveDelivered = zones[i].allocated - zones[i].lossAmount;
        double shortfall = zones[i].requested - zones[i].effectiveDelivered;
        zones[i].shortage = (shortfall > 0) ? shortfall : 0;
    }
}

void determineServiceConditions(Zone zones[], int n) {
    for (int i = 0; i < n; i++) {
        if (zones[i].allocated <= 0) {
            strcpy(zones[i].condition, "Unserved");
        } else if (zones[i].effectiveDelivered + 0.001 >= zones[i].requested) {
            strcpy(zones[i].condition, "Full requirement satisfied");
        } else if (zones[i].effectiveDelivered + 0.001 >= zones[i].minRequirement) {
            strcpy(zones[i].condition, "Minimum requirement satisfied");
        } else {
            strcpy(zones[i].condition, "Below minimum requirement");
        }
    }
}

int linearSearchByID(const Zone zones[], int n, const char *id) {
    for (int i = 0; i < n; i++) {
        if (strcmp(zones[i].zoneID, id) == 0) return i;
    }
    return -1;
}

void sortByShortageDesc(Zone zones[], int n, Zone sorted[]) {
    for (int i = 0; i < n; i++) sorted[i] = zones[i];

    for (int i = 1; i < n; i++) {
        Zone key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j].shortage < key.shortage) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
}


int findHighestUnresolved(const Zone zones[], int n) {
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (zones[i].shortage > zones[best].shortage) best = i;
    }
    return best;
}

void printAllocationSummary(const Zone zones[], int n, double availableWater) {
    printf("\n------------------------------------------------------------------------------------------------------------------\n");
    printf("ALLOCATION SUMMARY  (Total available at distribution centre: %.1f L)\n", availableWater);
    printf("------------------------------------------------------------------------------------------------------------------\n");
    printf("%-4s %-22s %9s %9s %5s %10s %10s %9s %-28s\n",
           "ID", "Service Zone", "Requested", "MinReq", "Rank", "Allocated",
           "Effective", "Shortage", "Service Condition");
    printf("------------------------------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        printf("%-4s %-22s %9.1f %9.1f %5d %10.1f %10.1f %9.1f %-28s\n",
               zones[i].zoneID, zones[i].zoneName, zones[i].requested,
               zones[i].minRequirement, zones[i].priorityRank,
               zones[i].allocated, zones[i].effectiveDelivered,
               zones[i].shortage, zones[i].condition);
    }
    printf("------------------------------------------------------------------------------------------------------------------\n");
}


void printTotalsAndCounts(const Zone zones[], int n, double availableWater) {
    double totalRequested = 0, totalAllocated = 0, totalEffective = 0, totalShortage = 0;
    int fullCount = 0, minOnlyCount = 0, belowMinCount = 0, unservedCount = 0;

    for (int i = 0; i < n; i++) {
        totalRequested += zones[i].requested;
        totalAllocated += zones[i].allocated;
        totalEffective += zones[i].effectiveDelivered;
        totalShortage  += zones[i].shortage;

        if (strcmp(zones[i].condition, "Full requirement satisfied") == 0) fullCount++;
        else if (strcmp(zones[i].condition, "Minimum requirement satisfied") == 0) minOnlyCount++;
        else if (strcmp(zones[i].condition, "Below minimum requirement") == 0) belowMinCount++;
        else unservedCount++;
    }

    double remainingAtCentre = availableWater - totalAllocated;

    printf("\nOVERALL TOTALS\n");
    printf("  Total water requested            : %.1f L\n", totalRequested);
    printf("  Total water allocated (released)  : %.1f L\n", totalAllocated);
    printf("  Total effective water delivered   : %.1f L\n", totalEffective);
    printf("  Total unresolved requirement      : %.1f L\n", totalShortage);
    printf("  Remaining water at centre         : %.1f L\n", remainingAtCentre);
    printf("\nSERVICE CONDITION COUNTS\n");
    printf("  Zones fully satisfied              : %d\n", fullCount);
    printf("  Zones at minimum only               : %d\n", minOnlyCount);
    printf("  Zones below minimum                 : %d\n", belowMinCount);
    printf("  Zones completely unserved           : %d\n", unservedCount);
}


void editZoneData(Zone zones[], int n) {
    char id[ID_LEN];
    printf("Enter Zone ID to edit (e.g. Z04): ");
    scanf("%7s", id);
    int idx = linearSearchByID(zones, n, id);
    if (idx < 0) {
        printf("Zone ID not found. No changes made.\n");
        return;
    }
    printf("Zone found: %s\n", zones[idx].zoneName);
    printf("Current distribution loss: %.1f %% | Current waiting cycles: %d\n",
           zones[idx].lossPercent, zones[idx].waitingCycles);
    printf("Enter new distribution loss %% (or -1 to keep unchanged): ");
    double newLoss;
    scanf("%lf", &newLoss);
    if (newLoss >= 0) zones[idx].lossPercent = newLoss;

    printf("Enter new waiting cycles (or -1 to keep unchanged): ");
    int newWaiting;
    scanf("%d", &newWaiting);
    if (newWaiting >= 0) zones[idx].waitingCycles = newWaiting;

    printf("Zone %s updated.\n", zones[idx].zoneID);
}
