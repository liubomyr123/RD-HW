#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <cmath>
#include <unordered_map>

#define LOG_INFO(x)     std::cout << x << "\n"
#define LOG_PROCESS(x)  std::cout << "🔄 " << x << "\n"
#define LOG_ERROR(x)    std::cerr << "❌ " << x << "\n"
#define LOG_WARN(x)     std::cout << "⚠️  " << x << "\n"
#define LOG_SUCCESS(x)  std::cout << "✅ " << x << "\n"

#define GRAVITATIONAL_ACCELERATION 9.81f
#define TARGET_COUNT 5
#define TARGET_TIME_STEPS 60

struct SimState {
    float totalSimTime = 0.0f;

    float droneX;
    float droneY;
    float droneZ;

    float droneDir;
    float droneVelocity = 0.0f;
};

/**
 * Написати консольну C++ програму, яка виконує покрокову симуляцію руху дрона до точки скиду боєприпасу на одну з 5 рухомих цілей. 
 * Програма обирає найближчу за часом ціль, враховує зміщення цілей, коригує курс на випередження (lead targeting) і моделює розгін, гальмування та повороти дрона.
 * Програма базується на ДЗ 1 (балістична задача) і розширює її циклами, масивами, enum та функціями.
 * 
 * Програма виводить дані у файл simulation.txt у форматі:
 * 
 *      | Рядок | Тип                   | Опис                  |
 *      ---------------------------------------------------------
 *      | 1     | N                     | кількість кроків      |
 *      | 2     | x0 y0 x1 y1 ... xN yN | координати дрона      |
 *      | 3     | d0 d1 ... dN          | напрямки, рад         |
 *      | 4     | s0 s1 ... sN          | стани: 0-4            |
 *      | 5     | t0 t1 ... tN          | індекси поточної цілі |
 * 
 * MAX_STEPS = 10000. Записується тільки до моменту скиду
 */

/**
 *      | Параметр          | Тип    | Опис                                                 |
 *      -------------------------------------------------------------------------------------
 *      | xd, yd, zd        | float  | Координати дрона (zd — висота, м)                    |
 *      | initialDir        | float  | Початковий напрямок дрона (радіани, від осі X)       |
 *      | attackSpeed       | float  | Швидкість атаки дрона (м/с)                          |
 *      | accelerationPath  | float  | Довжина розгону/гальмування (м)                      |
 *      | ammo_name         | char[] | Назва боєприпасу                                     |
 *      | arrayTimeStep     | float  | Крок часу масиву координат цілей (с)                 |
 *      | simTimeStep       | float  | Крок часу симуляції (с)                              |
 *      | hitRadius         | float  | Радіус ураження — допустима похибка попадання (м)    |
 *      | angularSpeed      | float  | Кутова швидкість повороту (рад/с)                    |
 *      | turnThreshold     | float  | Пороговий кут для зупинки (рад)                      |
 */

struct InputData {
    float xd; 
    float yd; 
    float zd;
    float initialDir; 
    float attackSpeed;
    float accelerationPath;
    float arrayTimeStep;
    float simTimeStep;
    float hitRadius;
    float angularSpeed;
    float turnThreshold;
    std::string ammo_name;
};

struct TargetsData {
    float targetXInTime[TARGET_COUNT][TARGET_TIME_STEPS];
    float targetYInTime[TARGET_COUNT][TARGET_TIME_STEPS];
};

struct OutputData {
    float fireX;
    float fireY;
    float postManeuverX;
    float postManeuverY;
    bool isRecalculated;
};

/**
 * Координати 5 цілей зчитуються з файлу targets.txt
 * targets.txt складається з 10 рядків: 
 *      - 5 рядків X-координат (60 значень через пробіл)
 *      - 5 рядків Y-координат (60 значень через пробіл)
 * 
 * Іншими словами:
 *      Рядок 1  -> targetXInTime[0][0..59]     (X для цілі 0)
 *      Рядок 2  -> targetXInTime[1][0..59]     (X для цілі 1)
 *      Рядок 3  -> targetXInTime[2][0..59]     ...
 *      Рядок 4  -> targetXInTime[3][0..59]     
 *      Рядок 5  -> targetXInTime[4][0..59]     
 * 
 *      Рядок 6  -> targetYInTime[0][0..59]     (Y для цілі 0)
 *      Рядок 7  -> targetYInTime[1][0..59]     ...
 *      Рядок 8  -> targetYInTime[2][0..59]     
 *      Рядок 9  -> targetYInTime[3][0..59]     
 *      Рядок 10 -> targetYInTime[4][0..59]    
 * 
 * І ось так можна це розуміти:
 *      - targetXInTime[0][0]
 *      - targetYInTime[0][0] 
 * Це координати 1-ї цілі на 1-му кроці.
 * 
 * Ще приклад.
 *      - targetXInTime[2][8]
 *      - targetYInTime[2][8] 
 * Це координати 3-ї цілі на 9-му кроці.
 *
 * Інтерполяція між кроками.
 * Інтерполяція - це знаходження значення між двома відомим точками.
 * Наприклад, ми знаємо яка температура була в 10:00 та 11:00.
 *      - 10:00 -> 20 градуси
 *      - 11:00 -> 24 градуси
 * Інтерполяція дозволяє оцінити температуру в 10:30 (наприклад, ~22 градуси).
 * 
 * Тепер застосуємо це в нашій задачі.
 * У нас є координати цілей в часі, наприклад кожні 5 секунд (при arrayTimeStep = 5 сек).
 * Тобто часовий відрізок між:
 *      - targetXInTime[0][0], targetYInTime[0][0]
 * та
 *      - targetXInTime[0][1], targetYInTime[0][1]
 * становить 5 секунд.
 * 
 * Але інформацію про дрон ми маємо не раз в 5 секунд, а наприклад кожні 0.1 секунди (simTimeStep = 0.1 сек).
 * І так як і ціль, і дрон рухаються, без інтерполяції ціль для дрона буде рухатися
 * не плавно, а ривками.
 * Тому нам потрібно обчислювати проміжні значення між відомими точками.
 * 
 * В даному завданні ми використовуємо лінійну інтерполяцію. Це означає що
 * між двома сусідніми точками відбувається рівномірний рух, тобто швидкість є стабільна, без різких стрибків чи затримок. 
 * Але швидкість може змінюватися при переході до наступних 2 точок.
 *
 * Розглянемо тепер нашу формулу:
 * int idx = (int)floor(t / arrayTimeStep) % 60;
 * int next = (idx + 1) % 60;
 * float frac = (t - idx * arrayTimeStep) / arrayTimeStep;
 * float x = targetX[i][idx] + (targetX[i][next] - targetX[i][idx]) * frac;
 * 
 * Нехай arrayTimeStep - 5 секунд. Тобто час між координатами цілей становить 5 секунд.
 * Далі ми маємо t. Це час симуляції simTimeStep = 0.1 сек. Тобто кожний момент часу дрона.
 * t = 0.0
 * t = 0.1
 * t = 0.2
 * t = 0.3
 * ...
 * 
 * Загалом ми будемо розраховувати проміжну точку між targetXInTime[0][0] та 
 * targetXInTime[0][1] для t - поточного моменту симуляції.
 * 
 * Отже, спочатку ми знаходимо idx. Це індекс який показує в якому часовому відрізку зараз знаходиться симуляція.
 * 
 * Нехай:
 * t = 0.4
 * arrayTimeStep = 5
 * 
 * 0.4 / 5 = 0.08
 * floor(0.08) = 0
 * idx = 0
 * Це вказує що ми знаходимося між targetXInTime[і][0] та targetXInTime[і][1].
 * 
 * Нехай:
 * t = 6.2
 * arrayTimeStep = 5
 * 
 * 6.2 / 5 = 1.24
 * floor(1.24) = 1
 * idx = 1
 * Це вказує що ми знаходимося між targetXInTime[0][1] та targetXInTime[0][2].
 * 
 * Тобто idx показує номер лівої точки інтервалу часу, 
 * а реальне положення знаходиться між targetXInTime[i][idx] та targetXInTime[i][idx + 1], де i — номер цілі.
 * 
 * Наступне - знаходимо next.
 * Це просто наступне значення після елемента під індексом idx.
 * Тобто якщо наприклад idx = 4, тоді next = 5.
 * Ми використовуємо (% 60) для того щоб рухатися по колу в масиві, не виходити за межі [0...59]
 * 
 * Йдемо далі і шукаємо frac.
 * Ця змінна показує, наскільки ми просунулися між точками під індексами idx та next.
 * Тобто це частка проходження інтервалу часу між двома відомими точками.
 * 
 * Наприклад:
 * 
 * t = 6.2, arrayTimeStep = 5
 * Як ми уже вирахували - idx буде рівний 1
 * 
 * frac = (t - idx * arrayTimeStep) / arrayTimeStep
 * frac = (6.2 - 1 * 5) / 5 = 0.24
 * 
 * Це означає, що ми пройшли 24% інтервалу часу між точками під індексами idx та next.
 * 
 * Ну і нарешті сама інтерполяція.
 * float x = targetX[i][idx] + (targetX[i][next] - targetX[i][idx]) * frac;
 * 
 * Як бачимо тут ми знаходимо координату Х між двома точками на поточний момент часу симуляції.
 * 
 * Тобто, 
 *      - targetX[i][idx]                       -> це стартова точка (ліва межа інтервалу)
 *      - (targetX[i][next] - targetX[i][idx])  -> це відстань між точками
 *      - множення відстані на frac             -> ми беремо чатину від цієї відстані
 *      - додавання координати targetX[i][idx]  -> це зсув від початкової точки
 * 
 * Приклад:
 *      targetX[i][next] = 100
 *      targetX[i][next] = 110
 *      frac = 0.3
 * 
 * Тоді інтерпляція буде:
 *      100 + (110 - 100) * 0.3 = 103
 * 
 * Отже, ми знайшли координату, в якій приблизно знаходиться ціль у даний момент часу симуляції.
 *  
 */

/**
 *      | Значення  | Назва         | Опис                                          |
 *      -----------------------------------------------------------------------------
 *      | 0         | STOPPED       | Повна зупинка (v = 0))                        |
 *      | 1         | ACCELERATING  | Розгін від 0 до attackSpeed                   |
 *      | 2         | DECELERATING  | Гальмування від attackSpeed до 0              |
 *      | 3         | TURNING       | Поворот на місці (v \= 0, зміна напрямку)     |
 *      | 4         | MOVING        | Рух з крейсерською швидкістю attackSpeed      |
 */

enum DronState
{
    STOPPED = 0,
    ACCELERATING,
    DECELERATING,
    TURNING,
    MOVING,
};

/**
 *     | Назва       | m (кг) | d (drag) | l (lift) | Тип               |
 *     ------------------------------------------------------------------
 *     | VOG-17      | 0.35   | 0.07     | 0.0      | Вільне падіння    |
 *     | M67         | 0.6    | 0.10     | 0.0      | Вільне падіння    |
 *     | RKG-3       | 1.2    | 0.10     | 0.0      | Вільне падіння    |
 *     | GLIDING-VOG | 0.45   | 0.10     | 1.0      | Планеруючий       |
 *     | GLIDING-RKG | 1.4    | 0.10     | 1.0      | Планеруючий       |
 * 
 *     m — маса боєприпасу (кг)
 *     d — коефіцієнт аеродинамічного опору
 *     l — коефіцієнт підйомної сили (0 = вільне падіння, 1 = планерування).
 */

struct AmmoInfo {
    float m;
    float d;
    float l;
    bool isFreeFall;
};

std::unordered_map<std::string, AmmoInfo> ammo_types_info = {
    {"VOG-17",      {0.35f, 0.07f, 0.0f, true}},
    {"M67",         {0.6f,  0.1f,  0.0f, true}},
    {"RKG-3",       {1.2f,  0.1f,  0.0f, true}},
    {"GLIDING-VOG", {0.45f, 0.1f,  1.0f, false}},
    {"GLIDING-RKG", {1.4f,  0.1f,  1.0f, false}},
};

bool getInputData(const std::string& file_name, InputData& inputData)
{
    LOG_PROCESS("Reading " + file_name + "...");

    std::ifstream file(file_name);
    if (!file) {
        LOG_ERROR("Error opening file");
        return false;
    }
    
    // Example: 
    /**
        150 150 100 - xd yd zd
        0           - initialDir
        10          - attackSpeed
        10          - accelerationPath
        VOG-17      - ammo_name
        5           - arrayTimeStep
        0.1         - simTimeStep
        3           - hitRadius
        1.0         - angularSpeed
        0.3         - turnThreshold
    */
    file >> inputData.xd >> inputData.yd >> inputData.zd 
        >> inputData.initialDir
        >> inputData.attackSpeed 
        >> inputData.accelerationPath 
        >> inputData.ammo_name
        >> inputData.arrayTimeStep
        >> inputData.simTimeStep
        >> inputData.hitRadius
        >> inputData.angularSpeed
        >> inputData.turnThreshold;

    if (file.fail()) {
        LOG_ERROR("Invalid format: wrong data types");
        return false;
    }
    else
    {
        LOG_SUCCESS("Successfully found all params.");
    }

    LOG_INFO("📄 Result:");
    LOG_INFO("  - xd: " << inputData.xd);
    LOG_INFO("  - yd: " << inputData.yd);
    LOG_INFO("  - zd: " << inputData.zd);
    LOG_INFO("  - initialDir: " << inputData.initialDir);
    LOG_INFO("  - attackSpeed: " << inputData.attackSpeed);
    LOG_INFO("  - accelerationPath: " << inputData.accelerationPath);
    LOG_INFO("  - ammo_name: " << inputData.ammo_name);
    LOG_INFO("  - arrayTimeStep: " << inputData.arrayTimeStep);
    LOG_INFO("  - simTimeStep: " << inputData.simTimeStep);
    LOG_INFO("  - hitRadius: " << inputData.hitRadius);
    LOG_INFO("  - angularSpeed: " << inputData.angularSpeed);
    LOG_INFO("  - turnThreshold: " << inputData.turnThreshold);

    std::string extra;
    if (file >> extra) {
        LOG_WARN("Found extra data: " + extra + " (ignored)");
    }

    return true;
}

bool getTargetsData(const std::string& file_name, TargetsData& targetsData)
{
    LOG_PROCESS("Reading " + file_name + "...");

    std::ifstream file(file_name);
    if (!file) {
        LOG_ERROR("Error opening file");
        return false;
    }

    // First 5 rows - x coordinates
    for (size_t i = 0; i < TARGET_COUNT; i++)
    {
        for (size_t j = 0; j < TARGET_TIME_STEPS; j++)
        {
            if (file >> targetsData.targetXInTime[i][j]) {
                // ...
            } else {
                LOG_ERROR("Failed reading X at target " << i << ", index " << j);
                return false;
            }
        }
    }

    // Next 5 rows - y coordinates
    for (size_t i = 0; i < TARGET_COUNT; i++)
    {
        for (size_t j = 0; j < TARGET_TIME_STEPS; j++)
        {
             if (file >> targetsData.targetYInTime[i][j]) {
                // ...
            } else {
                LOG_ERROR("Failed reading Y at target " << i << ", index " << j);
                return false;
            }
        }
    }
    
    if (file.fail()) {
        LOG_ERROR("Invalid format in targets file");
        return false;
    }

    LOG_SUCCESS("Successfully read all data for " << TARGET_COUNT << " targets.");

    return true;
}

bool getAmmoInfoByType(const std::string ammo_name, AmmoInfo& outAmmo)
{
    LOG_PROCESS("Searching ammo info for " << ammo_name << "...");

    auto it = ammo_types_info.find(ammo_name);
    
    if (it == ammo_types_info.end()) {
        LOG_WARN("Unknown ammo type: " << ammo_name);
        return false;
    }

    LOG_SUCCESS("Successfully found ammo type.");
    outAmmo = it->second;

    LOG_INFO("📄 Result:");
    LOG_INFO("  - m: " << outAmmo.m);
    LOG_INFO("  - d: " << outAmmo.d);
    LOG_INFO("  - l: " << outAmmo.l);
    LOG_INFO("  - isFreeFall: " << (outAmmo.isFreeFall ? "true" : "false"));

    return true;
}

// bool getAmmoTimeOfFlight(float& result, const InputData& inputData, const AmmoInfo& outAmmo)
// {
//     LOG_PROCESS("Calculating time of fly...");

//     float d = outAmmo.d;
//     float m = outAmmo.m;
//     float l = outAmmo.l;
//     float v0 = inputData.attackSpeed;
//     float z0 = inputData.zd;

//     // a · t³ + b · t² + c = 0
//     // a = d·g·m − 2d²·l·V₀
//     // b = −3g·m² + 3d·l·m·V₀
//     // c = 6m²·Z₀
//     // V₀ — швидкість атаки дрона, Z₀ — висота дрона (zd), g = 9.81 м/с².
//     float a = (d * GRAVITATIONAL_ACCELERATION * m) - 2 * d * d * l * v0;
//     if (std::abs(a) < 1e-6f)
//     {
//         LOG_ERROR("Invalid a: we cannot divide by zero");
//         return false;
//     }
//     float b = (-1) * 3 * GRAVITATIONAL_ACCELERATION * m * m + 3 * d * l * m * v0;
//     float c = 6 * m * m * z0;

//     // p = − b² / (3a²)
//     // q = 2b³ / (27a³) + c / a
//     // φ = arccos( 3q / (2p) · √(−3/p) )
//     float p = (-1) * b * b / (3 * a * a);
//     if (p >= 0.0f)
//     {
//         LOG_ERROR("Invalid p: must be negative for Cardano trig solution");
//         return false;
//     }
//     float q = 2 * b * b * b / (27 * a * a * a) + c / a;

//     float arg = (3 * q) / (2 * p) * std::sqrt(-3 / p);
//     if (arg < -1.0f || arg > 1.0f)
//     {
//         LOG_ERROR("Invalid acos argument: out of range");
//         return false;
//     }
//     float f = std::acos(arg);

//     // t = 2√(−p/3) · cos( (φ + 4π) / 3 ) − b / (3a)
//     float t = 2 * std::sqrt((p * (-1)) / 3) * std::cos((f + 4 * M_PI) / 3) - b / (3 * a);
//     LOG_SUCCESS("Successfully found time of flight");

//     LOG_INFO("📄 Result:");
//     LOG_INFO("  - t: " << t);

//     result = t;
//     return true;
// }

// bool getHorizontalFlightRange(float& result, const InputData& inputData, const AmmoInfo& outAmmo, const float& ammoTimeOfFlight)
// {
//     LOG_PROCESS("Calculating horizontal flight range...");

//     // h = V₀t 
//     //      − t²d·V₀/(2m)
//     //      + t³(6d·g·l·m − 6d²(l²-1)·V₀)/(36m²)
//     //      + t⁴(
//     //              −6d²g·l·(1+l²+l⁴)m
//     //              + 3d³l²(1+l²)V₀
//     //              + 6d³l⁴(1+l²)V₀
//     //          ) / (36(1+l²)²m³)
//     //      + t⁵(
//     //              3d³g·l³m
//     //              − 3d⁴l²(1+l²)V₀
//     //          ) / (36(1+l²)m⁴)

//     // V₀ — швидкість атаки дрона, Z₀ — висота дрона (zd), g = 9.81 м/с²
//     // t - час польоту снаряду (Time of Flight)

//     float t = ammoTimeOfFlight;
//     float d = outAmmo.d;
//     float m = outAmmo.m;
//     if (std::abs(m) < 1e-6f)
//     {
//         LOG_ERROR("Invalid m: we cannot divide by zero");
//         return false;
//     }
//     float l = outAmmo.l;
//     float v0 = inputData.attackSpeed;

//     float t2 = t * t;
//     float t3 = t2 * t;
//     float t4 = t2 * t2;
//     float t5 = t4 * t;

//     float d2 = d * d;
//     float d3 = d2 * d;
//     float d4 = d2 * d2;

//     float m2 = m * m;
//     float m3 = m2 * m;
//     float m4 = m2 * m2;

//     float l2 = l * l;
//     float l3 = l2 * l;
//     float l4 = l2 * l2;

//     // V₀t
//     float step0 = v0 * t;

//     // − t²d·V₀/(2m)
//     float step1 = ((-1) * t2 * d * v0) / (2 * m);

//     // + t³(6d·g·l·m − 6d²(l²-1)·V₀)/(36m²)
//     float step2 = t3 * ((6 * d * GRAVITATIONAL_ACCELERATION * l * m) - (6 * d2 * (l2 - 1) * v0)) / (36 * m2);

//     // −6d²g·l·(1+l²+l⁴)m
//     float step3_0 = (-1) * 6 * d2 * GRAVITATIONAL_ACCELERATION * l * (1 + l2 + l4) * m;
//     // + 3d³l²(1+l²)V₀
//     float step3_1 = 3 * d3 * l2 * (1 + l2) * v0;
//     // + 6d³l⁴(1+l²)V₀
//     float step3_2 = 6 * d3 * l4 * (1 + l2) * v0;
//     // 36(1+l²)²m³
//     float step3_3 = 36 * (1 + l2) * (1 + l2) * m3;

//     float step3 = t4 * (step3_0 + step3_1 + step3_2) / step3_3;

//     // 3d³g·l³m
//     float step4_0 = 3 * d3 * GRAVITATIONAL_ACCELERATION * l3 * m;
//     // − 3d⁴l²(1+l²)V₀
//     float step4_1 = (-1) * 3 * d4 * l2 * (1 + l2)* v0;
//     // 36(1+l²)m⁴
//     float step4_2 = 36 * (1 + l2) * m4;

//     float step4 = t5 * (step4_0 + step4_1) / step4_2;

//     float h = step0 + step1 + step2 + step3 + step4;

//     LOG_SUCCESS("Successfully calculated horizontal flight range");

//     LOG_INFO("📄 Result:");
//     LOG_INFO("  - h: " << h);

//     result = h;
//     return true;
// }

// bool getDistanceToTarget(float& result, const InputData& inputData)
// {
//     LOG_PROCESS("Calculating horizontal distance from copter to target...");

//     // D = √( (targetX − xd)² + (targetY − yd)² )
//     // targetX, targetY - координати цілі
//     // xd, yd - координати дрона

//     float xDiff = inputData.targetX - inputData.xd;
//     float yDiff = inputData.targetY - inputData.yd;

//     float step0 = xDiff * xDiff;
//     float step1 = yDiff * yDiff;

//     float D = std::sqrt(step0 + step1);

//     LOG_SUCCESS("Successfully calculated distance from drone to target");

//     LOG_INFO("📄 Result:");
//     LOG_INFO("  - D: " << D);

//     result = D;
//     return true;
// }

// bool isManeuverRequired(const float& h, const float& accelerationPath, const float& D)
// {
//     bool result = (h + accelerationPath) > D;
//     if (result)
//     {
//         LOG_WARN("Maneuver required: drone is too close to the target.");
//     }
//     else
//     {
//         LOG_SUCCESS("No maneuver required: drone is at correct release distance.");
//     }
//     return result;
// }

// bool getNewDroneCoordinatesForManeuver(float& newX, float& newY, const InputData& inputData, const float& D, const float& h)
// {
//     LOG_PROCESS("Calculating new drone coordinates for maneuver...");    

//     // xd' = targetX − (targetX − xd) · (h + accelerationPath) / D
//     // yd' = targetY − (targetY − yd) · (h + accelerationPath) / D

//     if (std::abs(D) < 1e-6f)
//     {
//         LOG_ERROR("Invalid D: we cannot divide by zero");
//         return false;
//     }

//     float step0 = (h + inputData.accelerationPath) / D;

//     newX = inputData.targetX - (inputData.targetX - inputData.xd) * step0;
//     newY = inputData.targetY - (inputData.targetY - inputData.yd) * step0;

//     LOG_SUCCESS("Successfully calculated new drone coordinates for maneuver.");

//     LOG_INFO("📄 Result:");
//     LOG_INFO("  - xd': " << newX);
//     LOG_INFO("  - yd': " << newY);

//     return true;
// }

// bool getAmmoDropPoint(OutputData& outputData, const InputData& inputData, const AmmoInfo& outAmmo, const float& D, const float& h)
// {
//     LOG_PROCESS("Calculating ammo drop point...");    

//     // ratio = (D − h) / D
//     // fireX = xd + (targetX − xd) · ratio
//     // fireY = yd + (targetY − yd) · ratio

//     if (std::abs(D) < 1e-6f)
//     {
//         LOG_ERROR("Invalid D: we cannot divide by zero");
//         return false;
//     }

//     float newXd = inputData.xd;
//     float newYd = inputData.yd;
//     float newH = h;
//     float newD = D;
//     auto newInputData = inputData;
//     if (isManeuverRequired(h, inputData.accelerationPath, D))
//     {
//         if (!getNewDroneCoordinatesForManeuver(newXd, newYd, inputData, D, h)) 
//         {
//             return false;
//         }

//         outputData.postManeuverX = newXd;
//         outputData.postManeuverY = newYd;
//         outputData.isRecalculated = true;

//         newInputData.xd = newXd;
//         newInputData.yd = newYd;

//         if (!getDistanceToTarget(newD, newInputData)) 
//         {
//             return false;
//         }
//         if (std::abs(newD) < 1e-6f)
//         {
//             LOG_ERROR("Invalid D: we cannot divide by zero");
//             return false;
//         }

//         float ammoTimeOfFlight = 0.0f;
//         if (!getAmmoTimeOfFlight(ammoTimeOfFlight, newInputData, outAmmo)) 
//         {
//             return false;
//         }

//         if (!getHorizontalFlightRange(newH, newInputData, outAmmo, ammoTimeOfFlight)) 
//         {
//             return false;
//         }
//     }

//     float ratio = (newD - newH) / newD;

//     outputData.fireX = newInputData.xd + (newInputData.targetX - newInputData.xd) * ratio;
//     outputData.fireY = newInputData.yd + (newInputData.targetY - newInputData.yd) * ratio;

//     LOG_SUCCESS("Successfully calculated ammo drop point.");

//     LOG_INFO("📄 Result:");
//     LOG_INFO("  - fireX: " << outputData.fireX);
//     LOG_INFO("  - fireY: " << outputData.fireY);

//     return true;
// }

// bool writeOutputToFile(const std::string& file_name, const OutputData& outputData)
// {
//     LOG_PROCESS("Writing result to " << file_name << "...");

//     std::ofstream file(file_name);
//     if (!file)
//     {
//         LOG_ERROR("Error opening file");
//         return false;
//     }

//     if (outputData.isRecalculated)
//     {
//         file << outputData.postManeuverX << " " << outputData.postManeuverY << " ";
//     }
//     file << outputData.fireX << " " << outputData.fireY;

//     if (file.fail())
//     {
//         LOG_ERROR("Failed while writing to file");
//         return false;
//     }

//     file.close();

//     LOG_SUCCESS("Successfully wrote result to file");
//     return true;
// }

struct InterpolationResult
{
    float resultX; 
    float resultY;
    size_t idx;
    size_t next;
    float frac;
};

bool interpolate(InterpolationResult& result, 
    const int targetIndex,
    const SimState& state, 
    const InputData& inputData, 
    const TargetsData& targetsData)
{
    /**
     * int idx = (int)floor(t / arrayTimeStep) % 60;
     * int next = (idx + 1) % 60;
     * float frac = (t - idx * arrayTimeStep) / arrayTimeStep;
     * float x = targetX[i][idx] + (targetX[i][next] - targetX[i][idx]) * frac;
     * 
     * t - конретний момент симуляції
     * arrayTimeStep - час між координатами цілей
     */

    if (std::abs(inputData.arrayTimeStep) < 1e-6f)
    {
        LOG_ERROR("Invalid arrayTimeStep: we cannot divide by zero");
        return false;
    }

    float t = state.totalSimTime;
    
    int idx = (int)std::floorf(t / inputData.arrayTimeStep) % 60;
    result.idx = idx;
    int next = (idx + 1) % 60;
    result.next = next;
    float frac = (t - idx * inputData.arrayTimeStep) / inputData.arrayTimeStep;
    result.frac = frac;

    float x0 = targetsData.targetXInTime[targetIndex][idx];
    float x1 = targetsData.targetXInTime[targetIndex][next];
    float x = x0 + (x1 - x0) * frac;
    result.resultX = x;

    float y0 = targetsData.targetYInTime[targetIndex][idx];
    float y1 = targetsData.targetYInTime[targetIndex][next];
    float y = y0 + (y1 - y0) * frac;
    result.resultY = y;

    LOG_SUCCESS("Successfully interpolated distance for target " << targetIndex);

    LOG_INFO("📄 Result:");
    LOG_INFO("  - x0: " << x0 << " -> " << "x1: " << x1);
    LOG_INFO("  - y0: " << y0 << " -> " << "y1: " << y1);
    LOG_INFO("  - New x for target: " << x);
    LOG_INFO("  - New y for target: " << y);
    LOG_INFO("  - Time interval between target snapshots: " << frac * 100 << "%");

    return true;
}

int main()
{
    std::string input_file_name = "input.txt";
    std::string output_file_name = "simulation.txt";
    std::string targets_file_name = "targets.txt";

    SimState state{};

    InputData inputData{};
    if (!getInputData(input_file_name, inputData))
    {
        return 1;
    }

    TargetsData targetsData{};
    if (!getTargetsData(targets_file_name, targetsData))
    {
        return 1;
    }

    state.droneX = inputData.xd;
    state.droneY = inputData.yd;
    state.droneZ = inputData.zd;

    state.droneDir = inputData.initialDir;

    int count = 1;
    while (count <= 5)
    {
        LOG_INFO("------ FRAME " << count << " ------");
        LOG_INFO("Total simulation time elapsed: " << state.totalSimTime << "[s]");

        for (size_t i = 0; i < 2; i++) // i < TARGET_COUNT;
        {
            InterpolationResult result{};
            if (!interpolate(result, i, state, inputData, targetsData))
            {
                return 1;
            }
        }

        // TODO: calculate distance from drone to each target on each frame

        state.totalSimTime = count * inputData.simTimeStep;
        count++;
        LOG_INFO("");
    }
    
    // AmmoInfo ammoInfo{};
    // if (!getAmmoInfoByType(inputData.ammo_name, ammoInfo)) 
    // {
    //     return 1;
    // }

    // float ammoTimeOfFlight = 0.0f;
    // if (!getAmmoTimeOfFlight(ammoTimeOfFlight, inputData, ammoInfo)) 
    // {
    //     return 1;
    // }

    // float horizontalFlightRange = 0.0f;
    // if (!getHorizontalFlightRange(horizontalFlightRange, inputData, ammoInfo, ammoTimeOfFlight)) 
    // {
    //     return 1;
    // }

    // float distanceToTarget = 0.0f;
    // if (!getDistanceToTarget(distanceToTarget, inputData)) 
    // {
    //     return 1;
    // }

    // OutputData outputData{};
    // if (!getAmmoDropPoint(outputData, inputData, ammoInfo, distanceToTarget, horizontalFlightRange)) 
    // {
    //     return 1;
    // }

    // if (!writeOutputToFile(output_file_name, outputData)) 
    // {
    //     return 1;
    // }

    return 0;
}
