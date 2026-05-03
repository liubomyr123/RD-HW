#include <iostream>
#include <fstream>
#include <sstream>
// #include <locale>
#include <string>
#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#endif

class Logger {
private:
    std::ofstream file;

    Logger() {
        file.open("app.log", std::ios::out | std::ios::trunc);
        // file.imbue(std::locale("")); // <- важливо
    }

public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(const std::string& type, const std::string& msg) {
        std::string line = type + " " + msg;

        std::cout << line << "\n";
        file << line << "\n";
    }
};

#define LOG_INFO(x) \
    do { \
        std::ostringstream oss; \
        oss << x; \
        Logger::instance().log("", oss.str()); \
    } while(0)

#define LOG_PROCESS(x) \
    do { \
        std::ostringstream oss; \
        oss << x; \
        Logger::instance().log("🔄", oss.str()); \
    } while(0)

#define LOG_ERROR(x) \
    do { \
        std::ostringstream oss; \
        oss << x; \
        Logger::instance().log("❌", oss.str()); \
    } while(0)

#define LOG_WARN(x) \
    do { \
        std::ostringstream oss; \
        oss << x; \
        Logger::instance().log("⚠️ ", oss.str()); \
    } while(0)

#define LOG_SUCCESS(x) \
    do { \
        std::ostringstream oss; \
        oss << x; \
        Logger::instance().log("✅", oss.str()); \
    } while(0)

#define GRAVITATIONAL_ACCELERATION 9.81f
#define TARGET_COUNT 5
#define TARGET_TIME_STEPS 60
#define SIM_MAX_STEPS 10000

/**
 *      | Значення  | Назва         | Опис                                          |
 *      -----------------------------------------------------------------------------
 *      | 0         | STOPPED       | Повна зупинка (v = 0))                        |
 *      | 1         | ACCELERATING  | Розгін від 0 до attackSpeed                   |
 *      | 2         | DECELERATING  | Гальмування від attackSpeed до 0              |
 *      | 3         | TURNING       | Поворот на місці (v \= 0, зміна напрямку)     |
 *      | 4         | MOVING        | Рух з крейсерською швидкістю attackSpeed      |
 */

enum DroneState
{
    STOPPED = 0,
    ACCELERATING,
    DECELERATING,
    TURNING,
    MOVING,
};

struct SimState {
    float totalSimTime = 0.0f;

    float droneX;
    float droneY;
    float droneZ;

    float droneDir;
    float droneVelocity = 0.0f;

    float angleTurnLeft = 0.0f;

    DroneState droneState = STOPPED;
    int currentTargetIndex = -1;
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

struct AmmoDropPointData {
    float fireX;
    float fireY;
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

struct Vec2 {
    float x;
    float y;
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
    // LOG_PROCESS("Reading " + file_name + "...");

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
        // LOG_SUCCESS("Successfully found all params.");
    }

    // Logger::instance().INFO("📄 Result:");
    LOG_INFO("📄 Result:");
    // Logger::instance().INFO("  - xd: " << inputData.xd);
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
    // LOG_PROCESS("Reading " + file_name + "...");

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

    // LOG_SUCCESS("Successfully read all data for " << TARGET_COUNT << " targets.");

    return true;
}

bool getAmmoInfoByType(const std::string ammo_name, AmmoInfo& ammoInfo)
{
    // LOG_PROCESS("Searching ammo info for " << ammo_name << "...");

    auto it = ammo_types_info.find(ammo_name);
    
    if (it == ammo_types_info.end()) {
        LOG_WARN("Unknown ammo type: " << ammo_name);
        return false;
    }

    ammoInfo = it->second;

    // LOG_SUCCESS("Successfully found ammo type.");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - m: " << ammoInfo.m);
    LOG_INFO("  - d: " << ammoInfo.d);
    LOG_INFO("  - l: " << ammoInfo.l);
    LOG_INFO("  - isFreeFall: " << (ammoInfo.isFreeFall ? "true" : "false"));

    return true;
}

bool getAmmoTimeOfFlight(float& result, 
    const InputData& inputData, 
    const AmmoInfo& ammoInfo)
{
    // LOG_PROCESS("Calculating time of fly...");

    float d = ammoInfo.d;
    float m = ammoInfo.m;
    float l = ammoInfo.l;
    float v0 = inputData.attackSpeed;
    float z0 = inputData.zd;

    // a · t³ + b · t² + c = 0
    // a = d·g·m − 2d²·l·V₀
    // b = −3g·m² + 3d·l·m·V₀
    // c = 6m²·Z₀
    // V₀ — швидкість атаки дрона, Z₀ — висота дрона (zd), g = 9.81 м/с².
    float a = (d * GRAVITATIONAL_ACCELERATION * m) - 2 * d * d * l * v0;
    if (std::abs(a) < 1e-6f)
    {
        LOG_ERROR("Invalid a: we cannot divide by zero");
        return false;
    }
    float b = (-1) * 3 * GRAVITATIONAL_ACCELERATION * m * m + 3 * d * l * m * v0;
    float c = 6 * m * m * z0;

    // p = − b² / (3a²)
    // q = 2b³ / (27a³) + c / a
    // φ = arccos( 3q / (2p) · √(−3/p) )
    float p = (-1) * b * b / (3 * a * a);
    if (p >= 0.0f)
    {
        LOG_ERROR("Invalid p: must be negative for Cardano trig solution");
        return false;
    }
    float q = 2 * b * b * b / (27 * a * a * a) + c / a;

    float arg = (3 * q) / (2 * p) * std::sqrt(-3 / p);
    if (arg < -1.0f || arg > 1.0f)
    {
        LOG_ERROR("Invalid acos argument: out of range");
        return false;
    }
    float f = std::acos(arg);

    // t = 2√(−p/3) · cos( (φ + 4π) / 3 ) − b / (3a)
    float t = 2 * std::sqrt((p * (-1)) / 3) * std::cos((f + 4 * M_PI) / 3) - b / (3 * a);

    // LOG_SUCCESS("Successfully found time of flight");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - t: " << t);

    result = t;
    return true;
}

bool getHorizontalFlightRange(float& result, 
    const InputData& inputData, 
    const AmmoInfo& ammoInfo, 
    const float& ammoTimeOfFlight)
{
    // LOG_PROCESS("Calculating horizontal flight range...");

    // h = V₀t 
    //      − t²d·V₀/(2m)
    //      + t³(6d·g·l·m − 6d²(l²-1)·V₀)/(36m²)
    //      + t⁴(
    //              −6d²g·l·(1+l²+l⁴)m
    //              + 3d³l²(1+l²)V₀
    //              + 6d³l⁴(1+l²)V₀
    //          ) / (36(1+l²)²m³)
    //      + t⁵(
    //              3d³g·l³m
    //              − 3d⁴l²(1+l²)V₀
    //          ) / (36(1+l²)m⁴)

    // V₀ — швидкість атаки дрона, Z₀ — висота дрона (zd), g = 9.81 м/с²
    // t - час польоту снаряду (Time of Flight)

    float t = ammoTimeOfFlight;
    float d = ammoInfo.d;
    float m = ammoInfo.m;
    if (std::abs(m) < 1e-6f)
    {
        LOG_ERROR("Invalid m: we cannot divide by zero");
        return false;
    }
    float l = ammoInfo.l;
    float v0 = inputData.attackSpeed;

    float t2 = t * t;
    float t3 = t2 * t;
    float t4 = t2 * t2;
    float t5 = t4 * t;

    float d2 = d * d;
    float d3 = d2 * d;
    float d4 = d2 * d2;

    float m2 = m * m;
    float m3 = m2 * m;
    float m4 = m2 * m2;

    float l2 = l * l;
    float l3 = l2 * l;
    float l4 = l2 * l2;

    // V₀t
    float step0 = v0 * t;

    // − t²d·V₀/(2m)
    float step1 = ((-1) * t2 * d * v0) / (2 * m);

    // + t³(6d·g·l·m − 6d²(l²-1)·V₀)/(36m²)
    float step2 = t3 * ((6 * d * GRAVITATIONAL_ACCELERATION * l * m) - (6 * d2 * (l2 - 1) * v0)) / (36 * m2);

    // −6d²g·l·(1+l²+l⁴)m
    float step3_0 = (-1) * 6 * d2 * GRAVITATIONAL_ACCELERATION * l * (1 + l2 + l4) * m;
    // + 3d³l²(1+l²)V₀
    float step3_1 = 3 * d3 * l2 * (1 + l2) * v0;
    // + 6d³l⁴(1+l²)V₀
    float step3_2 = 6 * d3 * l4 * (1 + l2) * v0;
    // 36(1+l²)²m³
    float step3_3 = 36 * (1 + l2) * (1 + l2) * m3;

    float step3 = t4 * (step3_0 + step3_1 + step3_2) / step3_3;

    // 3d³g·l³m
    float step4_0 = 3 * d3 * GRAVITATIONAL_ACCELERATION * l3 * m;
    // − 3d⁴l²(1+l²)V₀
    float step4_1 = (-1) * 3 * d4 * l2 * (1 + l2)* v0;
    // 36(1+l²)m⁴
    float step4_2 = 36 * (1 + l2) * m4;

    float step4 = t5 * (step4_0 + step4_1) / step4_2;

    float h = step0 + step1 + step2 + step3 + step4;

    // LOG_SUCCESS("Successfully calculated horizontal flight range");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - h: " << h << "[m]");

    result = h;
    return true;
}

bool getDistanceToTarget(float& result, 
    const float& droneX,
    const float& droneY,
    const float& targetX, 
    const float& targetY)
{
    // LOG_PROCESS("Calculating horizontal distance from copter to target...");

    // D = √( (targetX − xd)² + (targetY − yd)² )
    // targetX, targetY - координати цілі
    // xd, yd - координати дрона

    float deltaX = targetX - droneX;
    float deltaY = targetY - droneY;

    float step0 = deltaX * deltaX;
    float step1 = deltaY * deltaY;

    float D = std::sqrt(step0 + step1);

    // LOG_SUCCESS("Successfully calculated distance from drone to target");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - D: " << D << "[m]");

    result = D;
    return true;
}

bool isManeuverRequired(const float& h, 
    const float& accelerationPath, 
    const float& D)
{
    bool result = (h + accelerationPath) > D;
    if (result)
    {
        LOG_WARN("Maneuver required: drone is too close to the target.");
    }
    else
    {
        LOG_SUCCESS("No maneuver required: drone is at correct release distance.");
    }
    return result;
}

bool getNewDroneCoordinatesForManeuver(float& newX, 
    float& newY, 
    const InputData& inputData, 
    const float& D, 
    const float& h, 
    const float& targetX, 
    const float& targetY)
{
    // LOG_PROCESS("Calculating new drone coordinates for maneuver...");    

    // xd' = targetX − (targetX − xd) · (h + accelerationPath) / D
    // yd' = targetY − (targetY − yd) · (h + accelerationPath) / D

    if (std::abs(D) < 1e-6f)
    {
        LOG_ERROR("Invalid D: we cannot divide by zero");
        return false;
    }

    float step0 = (h + inputData.accelerationPath) / D;

    newX = targetX - (targetX - newX) * step0;
    newY = targetY - (targetY - newY) * step0;

    // LOG_SUCCESS("Successfully calculated new drone coordinates for maneuver.");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - xd': " << newX);
    LOG_INFO("  - yd': " << newY);

    return true;
}

bool getAmmoDropPoint(AmmoDropPointData& result, 
    // const InputData& inputData, 
    const float& droneX,
    const float& droneY,
    // const AmmoInfo& ammoInfo, 
    const float& D, 
    const float& h, 
    const float& targetX, 
    const float& targetY)
{
    // LOG_PROCESS("Calculating ammo drop point...");    

    // ratio = (D − h) / D
    // fireX = xd + (targetX − xd) · ratio
    // fireY = yd + (targetY − yd) · ratio

    if (std::abs(D) < 1e-6f)
    {
        LOG_ERROR("Invalid D: we cannot divide by zero");
        return false;
    }

    // float newXd = droneX;
    // float newYd = droneY;
    // float newH = h;
    // float newD = D;

    // if (isManeuverRequired(h, inputData.accelerationPath, D))
    // {
    //     if (!getNewDroneCoordinatesForManeuver(newXd, newYd, inputData, D, h, targetX, targetY)) 
    //     {
    //         return false;
    //     }

    //     if (!getDistanceToTarget(newD, newXd, newYd, targetX, targetY)) 
    //     {
    //         return false;
    //     }
    //     if (std::abs(newD) < 1e-6f)
    //     {
    //         LOG_ERROR("Invalid D: we cannot divide by zero");
    //         return false;
    //     }

    //     float ammoTimeOfFlight = 0.0f;
    //     if (!getAmmoTimeOfFlight(ammoTimeOfFlight, inputData, ammoInfo)) 
    //     {
    //         return false;
    //     }

    //     if (!getHorizontalFlightRange(newH, inputData, ammoInfo, ammoTimeOfFlight)) 
    //     {
    //         return false;
    //     }
    // }

    float ratio = (D - h) / D;

    result.fireX = droneX + (targetX - droneX) * ratio;
    result.fireY = droneY + (targetY - droneY) * ratio;

    // LOG_SUCCESS("Successfully calculated ammo drop point.");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - fireX: " << result.fireX);
    LOG_INFO("  - fireY: " << result.fireY);

    return true;
}

bool interpolate(Vec2& result, 
    const int& targetIndex,
    const float& timeFrame,
    const InputData& inputData, 
    const TargetsData& targetsData)
{
    // LOG_PROCESS("Calculating new interpolated position...");

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

    float t = timeFrame;
    
    int idx = (int)std::floorf(t / inputData.arrayTimeStep) % TARGET_TIME_STEPS;
    int next = (idx + 1) % TARGET_TIME_STEPS;
    float frac = (t - idx * inputData.arrayTimeStep) / inputData.arrayTimeStep;

    float x0 = targetsData.targetXInTime[targetIndex][idx];
    float x1 = targetsData.targetXInTime[targetIndex][next];
    float x = x0 + (x1 - x0) * frac;
    result.x = x;

    float y0 = targetsData.targetYInTime[targetIndex][idx];
    float y1 = targetsData.targetYInTime[targetIndex][next];
    float y = y0 + (y1 - y0) * frac;
    result.y = y;

    // LOG_SUCCESS("Successfully interpolated distance for target " << targetIndex);

    LOG_INFO("📄 Result:");
    LOG_INFO("  - x0: " << x0 << " -> " << "x1: " << x1);
    LOG_INFO("  - y0: " << y0 << " -> " << "y1: " << y1);
    LOG_INFO("  - New x for target: " << x);
    LOG_INFO("  - New y for target: " << y);
    LOG_INFO("  - Time interval between target snapshots: " << frac * 100 << "%");

    return true;
}

bool getTargetVelocity(Vec2& result, 
    const int targetIndex,
    const SimState& state, 
    const InputData& inputData, 
    const TargetsData& targetsData)
{
    // LOG_PROCESS("Calculating target velocity...");

    /**
     * float dx = targetX(t + dt) - targetX(t);
     * float dy = targetY(t + dt) - targetY(t);
     * float targetVx = dx / dt;
     * float targetVy = dy / dt;
     * 
     * t - конретний момент симуляції
     * dt - малий крок (наприклад, simTimeStep або arrayTimeStep)
     * targetX(t) - знайти Х координату де буде ціль в момент часу t
     * targetX(t + dt) - знайти Х координату де буде ціль через dt секунд від моменту t
     */

    float t = state.totalSimTime;
    float dt = inputData.arrayTimeStep;

    if (std::abs(dt) < 1e-6f)
    {
        LOG_ERROR("Invalid arrayTimeStep: we cannot divide by zero");
        return false;
    }

    float timeFrame0 = t;
    float timeFrame1 = t + dt;

    Vec2 currentPos{};
    if (!interpolate(currentPos, targetIndex, timeFrame0, inputData, targetsData))
    {
        return false;
    }
    Vec2 nextPos{};
    if (!interpolate(nextPos, targetIndex, timeFrame1, inputData, targetsData))
    {
        return false;
    }

    float x0 = currentPos.x;
    float x1 = nextPos.x;
    float dx = x1 - x0;

    float y0 = currentPos.y;
    float y1 = nextPos.y;
    float dy = y1 - y0;

    float targetVx = dx / dt;
    float targetVy = dy / dt;

    result.x = targetVx;
    result.y = targetVy;

    // LOG_SUCCESS("Successfully calculated velocity for target " << targetIndex);

    LOG_INFO("📄 Result:");
    LOG_INFO("  - Velocity x: " << targetVx);
    LOG_INFO("  - Velocity y: " << targetVy);

    return true;
}

bool getPredictedPosition(Vec2& result, 
    const float& targetVx, const float& targetVy, 
    const float& targetX, const float& targetY, 
    const float& totalTime)
{
    // LOG_PROCESS("Calculating predicted position...");

    /**
     * float predictedX = targetX(t) + targetVx * totalTime;
     * float predictedY = targetY(t) + targetVy * totalTime;
     * 
     * targetVx - швидкість цілі по осі x
     * targetVy - швидкість цілі по осі y
     * totalTime - орієнтовний час прильоту дрона до точки скиду
     */

    float predictedX = targetX + targetVx * totalTime;
    float predictedY = targetY + targetVy * totalTime;

    result.x = predictedX;
    result.y = predictedY;

    // LOG_SUCCESS("Successfully calculated predicted position");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - Predicted x: " << predictedX);
    LOG_INFO("  - Predicted y: " << predictedY);

    return true;
}

bool getTimeToTarget(float& result, 
    const float& distanceToTarget, 
    const float& speed)
{
    // LOG_PROCESS("Calculating drone travel time to target point...");

    if (std::abs(speed) < 1e-6f)
    {
        LOG_ERROR("Invalid speed: cannot divide by zero");
        return false;
    }

    result = distanceToTarget / speed;

    // LOG_SUCCESS("Successfully calculated time for distance");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - Distance to point: " << distanceToTarget << "[m]");
    LOG_INFO("  - Speed: " << speed << "[m/s]");
    LOG_INFO("  - Time required: " << result << "[s]");

    return true;
}

bool getClosestTargetIndexByTime(size_t& result, 
    const size_t& targets_number, 
    const float timesToDropPoint[])
{
    // LOG_PROCESS("Searching for target with minimal time to drop point...");

    if (targets_number == 0)
    {
        LOG_ERROR("No targets provided");
        return false;
    }

    float tempTime = timesToDropPoint[0];
    size_t closest = 0;
    for (size_t i = 1; i < targets_number; i++) 
    {
        float timeToDrop = timesToDropPoint[i];
        if (timeToDrop < tempTime)
        {
            tempTime = timeToDrop;
            closest = i;
        }
    }

    result = closest;

    // LOG_SUCCESS("Successfully found closest target by time to drop point");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - Target with minimal time to drop point: " << result);

    return true;
}

bool getDeltaAngle(float& result,
    const float& droneX, const float& droneY,
    const float& droneDir,
    const float& targetX, const float& targetY)
{
    // LOG_PROCESS("Calculating angle difference (drone → target)...");

    float dx = targetX - droneX;
    float dy = targetY - droneY;

    float targetAngle = std::atan2(dy, dx);
    float deltaAngle = targetAngle - droneDir;

    while (deltaAngle > M_PI)
        deltaAngle -= 2.0f * M_PI;

    while (deltaAngle < -M_PI)
        deltaAngle += 2.0f * M_PI;

    result = deltaAngle;

    // LOG_SUCCESS("Successfully calculated delta angle");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - Angle difference: " << result << "[rad]");

    return true;
}

float normalizeAngle(float a)
{
    while (a > M_PI) a -= 2.0f * (float)M_PI;
    while (a < -M_PI) a += 2.0f * (float)M_PI;
    return a;
}

bool getAcceleration(float& result, const InputData& inputData)
{
    // LOG_PROCESS("Calculating dron acceleration...");

    if (std::abs(inputData.accelerationPath) < 1e-6f)
    {
        LOG_ERROR("Invalid accelerationPath: cannot divide by zero");
        return false;
    }

    result = (inputData.attackSpeed * inputData.attackSpeed) / (2.0f * inputData.accelerationPath);

    // LOG_SUCCESS("Successfully calculated acceleration");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - Dron acceleration: " << result);

    return true;
}

// 1) Беремо поточні координати дрона і цілей
// 2) Для кожної цілі:
//      оцінюємо точку перехоплення (intercept point)
//      і рахуємо скільки часу потрібно дрону, щоб до неї дістатися
//      це і є totalTime (оцінка для цього фрейму)
// 3) Беремо totalTime і прогнозуємо де буде ціль через цей час
// 4) Перераховуємо точку перехоплення вже під прогнозовану позицію цілі
//      (тобто оновлюємо intercept point)
// 5) Вибираємо найкращу ціль (з мінімальним totalTime)
// 6) Дрон оновлює напрямок руху (поворот / прискорення / гальмування)
// 7) Дрон рухається вперед на simTimeStep
// 8) Переходимо до наступного фрейму
// 9) На наступному фреймі:
//      маємо нову позицію дрона
//      маємо нові (інтерпольовані) позиції цілей
// 10) Знову для кожної цілі:
//      рахуємо totalTime до нової точки перехоплення
// 11) Знову прогнозуємо позицію цілі через цей totalTime
// 12) Знову уточнюємо точку перехоплення
// 13) Продовжуємо симуляцію руху дрона
// 14) Ітеруємося до моменту:
//      коли дрон входить у hitRadius до точки скиду
//      і виконується скидання боєприпасу

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

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

    AmmoInfo ammoInfo{};
    if (!getAmmoInfoByType(inputData.ammo_name, ammoInfo)) 
    {
        return 1;
    }

    float ammoTimeOfFlight = 0.0f;
    if (!getAmmoTimeOfFlight(ammoTimeOfFlight, inputData, ammoInfo)) 
    {
        return 1;
    }

    float horizontalFlightRange = 0.0f;
    if (!getHorizontalFlightRange(horizontalFlightRange, inputData, ammoInfo, ammoTimeOfFlight)) 
    {
        return 1;
    }

    float acceleration = 0.0f;
    if (!getAcceleration(acceleration, inputData)) 
    {
        return 1;
    }

    state.droneX = inputData.xd;
    state.droneY = inputData.yd;
    state.droneZ = inputData.zd;

    state.droneDir = inputData.initialDir;
    state.droneState = STOPPED;

    const size_t targets_number = 1; // TARGET_COUNT

    int count = 1;
    while (count <= 3) // < SIM_MAX_STEPS
    {
        LOG_INFO("------ FRAME " << count << " ------");
        LOG_INFO("Total simulation time elapsed: " << state.totalSimTime << "[s]");

        // Знайшли координати цілей на даний момент
        Vec2 interpolatedPosition[targets_number]{};
        for (size_t i = 0; i < targets_number; i++)
        {
            if (!interpolate(interpolatedPosition[i], i, state.totalSimTime, inputData, targetsData))
            {
                return 1;
            }
        }

        // Шукаємо найкращу ціль на даний момент
        // Ітераційно пройтися та:
        // 1) розрахувати для кожної цілі точку скиду
        // 2) розрахувати час який треба дрону щоб долетіти до точки скиду + час польоту снаряду
        // 3) розрахувати саму позицію цілі
        // 4) вибрати ту ціль до якої треба найменше часу летіти
        float smallestTime = std::numeric_limits<float>::max();
        int bestTarget = -1;
        float bestTargetX = 0.0f;
        float bestTargetY = 0.0f;
        float bestDropPointX = 0.0f;
        float bestDropPointY = 0.0f;
        for (size_t target_iterator = 0; target_iterator < targets_number; target_iterator++) 
        {
            LOG_INFO("-----------: target # " << target_iterator);

            float predictedTargetX = 0.0f;
            float predictedTargetY = 0.0f;

            float predictedDropPointX = 0.0f;
            float predictedDropPointY = 0.0f;

            // Нехай перше припущення цілі - там де ціль зараз стоїть
            predictedTargetX = interpolatedPosition[target_iterator].x;
            predictedTargetY = interpolatedPosition[target_iterator].y;

            for (size_t j = 0; j < 6; j++)
            {
                LOG_INFO("-----------: prediction # " << j);

                // ✅ 1) Треба знайти точку скиду 
                // ✅ 2) Скільки часу дрон буде до неї летіти 
                // ✅ 3) Час польоту снаряду ми уже маємо - ammoTimeOfFlight 
                // ✅ 4) І знаючи час долітання до точки скиду та політ снаряду можемо знайти 
                //                      де буде ціль від поточного моменту через цей час
                // ✅ 5) Оновити прогнозоване місце цілі і повторити ітерацію
                // ✅ 6) В кінці ми отримаємо для цієї цілі нове місце цілі та точку скиду

                float distanceToTarget = 0.0f;
                if (!getDistanceToTarget(distanceToTarget, 
                    state.droneX, state.droneY, 
                    predictedTargetX, predictedTargetY)) 
                {
                    return 1;
                }

                AmmoDropPointData ammoDropPoint{};
                if (!getAmmoDropPoint(ammoDropPoint,
                    state.droneX, state.droneY, 
                    distanceToTarget, 
                    horizontalFlightRange, 
                    predictedTargetX, predictedTargetY)) 
                {
                    return 1;
                }
                predictedDropPointX = ammoDropPoint.fireX;
                predictedDropPointY = ammoDropPoint.fireY;

                float distanceToDropPoint = 0.0f;
                if (!getDistanceToTarget(distanceToDropPoint, 
                    state.droneX, state.droneY, 
                    ammoDropPoint.fireX, ammoDropPoint.fireY)) 
                {
                    return 1;
                }

                float timeToDropPoint = 0.0f;
                if (!getTimeToTarget(timeToDropPoint, distanceToDropPoint, inputData.attackSpeed)) 
                {
                    return 1;
                }

                float totalTime = timeToDropPoint + ammoTimeOfFlight;

                Vec2 targetVelocity{};
                if (!getTargetVelocity(targetVelocity, target_iterator, state, inputData, targetsData))
                {
                    return 1;
                }
                Vec2 predictedPosition{};
                if (!getPredictedPosition(predictedPosition, 
                    targetVelocity.x, targetVelocity.y, 
                    predictedTargetX, predictedTargetY,
                    totalTime)) 
                {
                    return 1;
                }

                predictedTargetX = predictedPosition.x;
                predictedTargetY = predictedPosition.y;
            }
        
            // Далі треба знайти timeToStop, тобто додатковий час необхідний якщо ми хочемо змінити ціль
            // Будемо рахувати штрафний час до зупинки
            float timeToStop = 0.0f;
            switch (state.droneState)
            {
                case STOPPED:
                    // Ми уже стоїмо і нам не треба додаткового часу
                    break;
                case ACCELERATING:
                    // Швидкість дрона >=0, тому нам треба врахувати скільки часу
                    // дрон уже розганяється, як штрафний час необхідний щоб зупинитися
                    // 
                    // Наприклад:
                    //      - поточна швидкість = 8 м/с
                    //      - прискорення - 5 м/с
                    // 
                    // Тоді час який дрон уже затратив на досягнення швидкості 8 м/с буде:
                    // 5 м/с - 1 с
                    // 8 м/с - х с
                    // х = (8 м/с) / (5 м/с) = 1.6 c
                    timeToStop = state.droneVelocity / acceleration;
                    break;
                case MOVING:
                    // Дрон уже рухається зі стабільною макс швидкістю, отже треба врахувати
                    // час на гальмування = attackSpeed / acceleration, ми беремо 
                    // attackSpeed бо ми уже на цій стабільній швидкості
                    timeToStop = inputData.attackSpeed / acceleration;
                    break;
                case TURNING:
                    // Дрон зараз розвертається, і він все ще в процесі
                    // Не важливо чи він почав, чи ще в процесі, залишається ще певний кут 
                    // який йому залишилося розвернутися до попередньої активної цілі
                    // Отже, нам треба знати скільки по часу ще залишилося йому розвертатися і зупинитися
                    // 
                    // Наприклад, нехай залишилося ще розвернути 1.2 радіани
                    // І маємо максимальну швидкість повороту - angularSpeed рад/с, нехай 1 рад/с
                    // Тоді час необхідний щоб довернути решту 1.2 радіанів буде:
                    // 1 рад    - 1 с
                    // 1.2 рад  - х с
                    // х = (1.2 рад) / (1 рад) = 1.2 с
                    timeToStop = state.angleTurnLeft / inputData.angularSpeed;
                    break;
                case DECELERATING:
                    // Час необхідний щоб повністю зупинитися, тобто скільки ще часу він буде тормозити
                    // Ми маємо його поточну швидкість, яка падає
                    // І можемо знайти скільки ще часу він буде сповільнюватися до 0
                    // За умовою, швидкість прискорення та гальмування є однаковим
                    // Тому робимо так само як і в ACCELERATING
                    timeToStop = state.droneVelocity / acceleration;
                    break;
                default:
                    break;
            }

            float distanceToDropPoint = 0.0f;
            if (!getDistanceToTarget(distanceToDropPoint, 
                state.droneX, state.droneY, 
                predictedDropPointX, predictedDropPointY)) 
            {
                return 1;
            }

            float timeToDropPoint = 0.0f;
            if (!getTimeToTarget(timeToDropPoint, distanceToDropPoint, inputData.attackSpeed)) 
            {
                return 1;
            }

            float totalTime = 0.0f;

            float distanceToTarget = 0.0f;
            if (!getDistanceToTarget(distanceToTarget, 
                state.droneX, state.droneY, 
                predictedTargetX, predictedTargetY)) 
            {
                return 1;
            }

            if (distanceToTarget >= horizontalFlightRange)
            {
                // Все добре, дрон є на достатній відстані від цілі щоб попасти снарядом
                totalTime = timeToDropPoint + ammoTimeOfFlight;
            }
            else
            {
                // Дрон перетнув межу horizontalFlightRange, треба врахувати час на маневр
                // Тому треба знайти скільки часу займе дрону на маневр: 
                //      - тобто пролетіти якусь додаткову дистанцію щоб мати можливість скинути
                //      - і можливо треба буде розвернутися до цілі, беремо найгірший випадок - розворот на 180 градусів
                // 
                // Нехай відстань від дрона до цілі - 5 метрів, а дистанція на скид - 7 метрів
                // Це означає, що дрон уже як 2 метри пропустив точку скиду
                // Тому йому треба відлетіти на ще на 5 метрів і ще як мінімум на 7 метрів, потім ще й розвернутися
                // 
                // 1 радіан = (180 градусів / M_PI)
                // 
                // Нехай у нас angularSpeed = 3 рад/с, тобто за 1 секунду ми повертаємось на 3 радіани або 3 * (180 градусів / M_PI)
                // Тоді щоб знайти час скільки дрон буде розвертатися на 180 градусів:
                // 
                // 1 сек - 3 * (180 градусів / M_PI)
                // х сек - 180 градусів
                // 
                // x сек = (180 градусів * 1 сек) / (3 * (180 градусів / M_PI))
                // x сек = ((180 градусів * 1 сек) / 1) * (M_PI / (3 * 180 градусів)) | скорочуємо на 180 градусів
                // x сек = ((1 сек) / 1) * (M_PI / (3))
                // x сек = M_PI / 3
                // Тобто час розвороту на 180 градусів - це M_PI / inputData.angularSpeed
                float timeOfFullTurn = (float)M_PI / inputData.angularSpeed;
                
                float restoreDistance = distanceToTarget + horizontalFlightRange;

                float restoreTime = 0.0f;
                if (!getTimeToTarget(restoreTime, restoreDistance, inputData.attackSpeed)) 
                {
                    return 1;
                }

                totalTime = restoreTime + timeOfFullTurn + ammoTimeOfFlight;
            }

            // Якщо це якась інша ціль, не та до якої ми зараз летимо, будемо враховувати штраф на зміну цілі
            if (state.currentTargetIndex != -1 && target_iterator != state.currentTargetIndex)
            {
                totalTime += timeToStop;
            }

            if (totalTime < smallestTime)
            {
                smallestTime = totalTime;
                bestTarget = target_iterator;
                bestTargetX = predictedTargetX;
                bestTargetY = predictedTargetY;
                bestDropPointX = predictedDropPointX;
                bestDropPointY = predictedDropPointY;
            }
        }
        LOG_INFO("Best target -> " << bestTarget);
        state.currentTargetIndex = bestTarget;

        LOG_INFO("bestTargetX -> " << bestTargetX);
        LOG_INFO("bestTargetY -> " << bestTargetY);

        float bestDistanceToTarget = 0.0f;
        if (!getDistanceToTarget(bestDistanceToTarget, 
            state.droneX, state.droneY, 
            bestTargetX, bestTargetY)) 
        {
            return 1;
        }

        if (bestDistanceToTarget >= horizontalFlightRange)
        {
            // Все добре, дрон є на достатній відстані від цілі щоб попасти снарядом
            // Точка скиду залишається без змін
        }
        else
        {
            // Дрон перетнув межу horizontalFlightRange, треба знову врахувати час на маневр
            // Треба знайти нову точку скиду, треба знайти точку на відстані horizontalFlightRange від цілі
            // Тобто у мене є координати цілі та дрона і відстань horizontalFlightRange
            // 
            // 1) Знайти вектор від дрона до цілі
            // 2) Знаходимо одиничний вектор
            // 3) Рухаємося на певну відстань по цьому 1-ому вектору
            // 
            // Наприклад,
            //      - дрон (x1 = 1, y1 = 2) 
            //      - ціль (x2 = 5, y2 = 4) 
            //      - треба відступити від цілі на 3 метри
            // 
            // 1) Знадемо векток від дрона до цілі:
            // Вектор рахуємо (куди - звідки):
            //      drone -> target => (targetX - droneX; targetY - droneY)
            //      target -> drone => (droneX - targetX; droneY - targetY)
            // 
            //      (x2 - x1; y2 - y1)
            //      (5-1; 4-2) = (4; 2)
            // 
            // 2) Знадемо одиничний вектор:
            //      а) Знайти довжину ветора: √((x2 - x1)² + (y2 - y1)²)
            //          √(4² + 2²) = √(16 + 4) = √(20) ~ 4.47
            //      б) Поділити координати на довжину:
            //          (4 / 4.47; 2 / 4.47) = (0.89; 0.44) = (nx; ny)
            // 3) Рухаємося на 3 метри від цілі:
            //          (x2 - nx * 3; y2 - ny * 3)
            //          (5 - 0.89 * 3; 4 - 0.44 * 3)
            //          (5 - 2.67; 4 - 1.32)
            //          (2.33; 2.68)
            // Отже, (2.33; 2.68) - це координати точки на відстані в 3 метри від цілі в напрямку дрона

            // 1)
            float vecX = bestTargetX - state.droneX;
            float vecY = bestTargetY - state.droneY;

            // 2)
            float vecDistance = std::sqrt((vecX * vecX) + (vecY * vecY));
            float nx = vecX / vecDistance;
            float ny = vecY / vecDistance;

            // 3)
            float newDropPointX = bestTargetX - nx * horizontalFlightRange;
            float newDropPointY = bestTargetY - ny * horizontalFlightRange;

            bestDropPointX = newDropPointX;
            bestDropPointY = newDropPointY;
        }

        // Тепер треба знайти чи треба нам скидутаи снаряд прямо зараз, щоб виконати місію
        // Для цього дрон має рухатися, тобто мати швидкість attackSpeed - швидкість при якій він скидає
        // І через те що і ціль рухається також нам треба знати де буде ціль якщо ми
        // прямо зараз в поточкому фреймі скинемо снаряд і воно пролетить ammoTimeOfFlight секунд часу
        // І порівняти 2 точки - де впаде снаряд і де буде ціль
        // В ідеалі якщо це однакові точки - ми фактично фіксуємо попадання
        // Але за умовою ми маємо hitRadius - це допустима похибка попадання в метрах
        // Отже, у нас буде певний запас на попадання цілі
        // Тому будемо шукати точку де буде ціль 
        // Шукаємо відстань від точки де впаде снаряд до точки де буде ціль і ця відстань
        // має бути меншою-рівною hitRadius
        // 1) Знаходимо одиничний вектор напрямку дрона:
        float vecX = std::cos(state.droneDir);
        float vecY = std::sin(state.droneDir);
        // 2) Рахуємо зміщення по координатах на horizontalFlightRange по знайдемону вектору
        float shiftX = vecX * horizontalFlightRange;
        float shiftY = vecY * horizontalFlightRange;
        // 3) Знаходимо точку де буде снаряд 
        float hitX = state.droneX + shiftX;
        float hitY = state.droneY + shiftY;
        // 4) Шукаємо точку де буде ціль ціль через ammoTimeOfFlight
        Vec2 targetVelocity{};
        if (!getTargetVelocity(targetVelocity, state.currentTargetIndex, state, inputData, targetsData))
        {
            return 1;
        }
        Vec2 predictedPosition{};
        if (!getPredictedPosition(predictedPosition, 
            targetVelocity.x, targetVelocity.y, 
            bestTargetX, bestTargetY,
            ammoTimeOfFlight)) 
        {
            return 1;
        }
        // 5) Шукаємо відстань між цими точками
        float hitDistance = 0.0f;
        if (!getDistanceToTarget(hitDistance, 
            hitX, hitY, 
            predictedPosition.x, predictedPosition.y)) 
        {
            return 1;
        }
        if (state.droneState == MOVING && hitDistance <= inputData.hitRadius)
        {
            // Ура, реєструємо ураження цілі, зупиняємо симуляцію
            break;
        }

        // Тепр коли ми маємо нову ціль + її координати + нову точку скиду
        // треба зрозуміти чи має дрон розвернутися до точки скиду
        // 1) Для цього знайдемо спочатку напрямок куди дрон має дивитися для нової цілі
        float vecX = bestDropPointX - state.droneX;
        float vecY = bestDropPointY - state.droneY;
        // 2) Далі треба знайти кут між Х і точкою скиду
        float angleToDropPoint = std::atan2(vecY, vecX);
        // angleToDropPoint - це значення в радіанах, і воно обмежене [-M_PI; M_PI] або [-180 градусів; 180 градуів]
        // 3) І тепер ми можемо знайти різницю між тим куди зараз дивитися дрон і тим куди треба
        float deltaAngle = normalizeAngle(angleToDropPoint - state.droneDir);
        // І тепер якщо deltaAngle = 0, це означає що дрон дивитися прямо на точку скиду
        // Якщо < 0, тоді треба розвертатися ліворуч, якщо > 0, тоді праворуч
        LOG_INFO("deltaAngle -> " << deltaAngle);

        state.totalSimTime = count * inputData.simTimeStep;
        count++;
        LOG_INFO("");
    }

    return 0;
}

        // if (absAngle <= inputData.turnThreshold)
        // { 
        //     LOG_INFO("Soft turn (no stop required)"); 
        //     float appliedTurn = deltaAngle; 
        //     if (appliedTurn > maxTrunStep) 
        //     { 
        //         appliedTurn = maxTrunStep;
        //     } 
        //     else if (appliedTurn < -maxTrunStep)
        //     { 
        //         appliedTurn = -maxTrunStep; 
        //     }
        //     state.droneDir += appliedTurn;
        //      state.droneState = MOVING;
        // } 
        // else
        // { 
        //     LOG_INFO("Hard turn (STOP required)"); 
        //     state.droneState = DECELERATING;
        // }





        
        // size_t closestTargetIndex = 0;
        // if (!getClosestTargetIndexByTime(closestTargetIndex, targets_number, timesToDropPoint)) 
        // {
        //     return 1;
        // }

        // state.currentTargetIndex = closestTargetIndex;

        // LOG_INFO("");
        // LOG_INFO("  - Best target index: " << closestTargetIndex);
        // LOG_INFO("  - Time to drop point: " << timesToDropPoint[closestTargetIndex]);
        // LOG_INFO("  - Distance to CURRENT target: " << targetDistances[closestTargetIndex]); 
        // LOG_INFO("");

        // float deltaAngle = 0;
        // if (!getDeltaAngle(deltaAngle, 
        //     state.droneX, state.droneY, 
        //     state.droneDir, 
        //     dropPoints[state.currentTargetIndex].x,
        //     dropPoints[state.currentTargetIndex].y)) 
        // {
        //     return 1;
        // }

        // const float maxTrunStep = inputData.angularSpeed * inputData.simTimeStep; // 1 * 0.1
        // const float absAngle = std::fabs(deltaAngle); 
        // const bool hardTurn = absAngle > inputData.turnThreshold;

        // LOG_INFO("");
        // LOG_INFO("  - Drone velocity: " << state.droneVelocity);
        // LOG_INFO("");

        // switch (state.droneState)
        // {
        //     case STOPPED:
        //     {
        //         LOG_INFO("Action STATE: STOPPED");

        //         state.droneVelocity = 0.0f;
        //         state.droneState = ACCELERATING;
        //         break;
        //     }
        //     case MOVING:
        //     {
        //         LOG_INFO("Action STATE: MOVING");

        //         if (hardTurn)
        //         {
        //             state.droneState = DECELERATING;
        //             break;
        //         }

        //         float stoppingDistance = (state.droneVelocity * state.droneVelocity) / 
        //                                 (2.0f * acceleration);

        //         if (dropDistances[state.currentTargetIndex] <= stoppingDistance)
        //         {
        //             state.droneState = DECELERATING;
        //             break;
        //         }

        //         float dt = inputData.simTimeStep;

        //         state.droneX += state.droneVelocity * std::cos(state.droneDir) * dt;
        //         state.droneY += state.droneVelocity * std::sin(state.droneDir) * dt;
        //         break;
        //     }
        //     case TURNING:
        //     {
        //         LOG_INFO("Action STATE: TURNING");

        //         float appliedTurn = deltaAngle; 
        //         if (appliedTurn > maxTrunStep) 
        //         { 
        //             appliedTurn = maxTrunStep;
        //         } 
        //         else if (appliedTurn < -maxTrunStep)
        //         { 
        //             appliedTurn = -maxTrunStep; 
        //         }

        //         state.droneDir += appliedTurn;

        //         float newDeltaAngle = 0;
        //         if (!getDeltaAngle(newDeltaAngle, 
        //             state.droneX, state.droneY, 
        //             state.droneDir, 
        //             dropPoints[state.currentTargetIndex].x,
        //             dropPoints[state.currentTargetIndex].y)) 
        //         {
        //             return 1;
        //         }

        //         if (std::fabs(newDeltaAngle) <= 0.00005f) // turnThreshold 
        //         {
        //             state.droneState = ACCELERATING;
        //         }
        //         break;
        //     }
        //     case ACCELERATING:
        //     {
        //         LOG_INFO("Action STATE: ACCELERATING");

        //         if (hardTurn)
        //         {
        //             state.droneState = DECELERATING;
        //             break;
        //         }

        //         float stoppingDistance = (state.droneVelocity * state.droneVelocity) / 
        //                                 (2.0f * acceleration);

        //         if (dropDistances[state.currentTargetIndex] <= stoppingDistance)
        //         {
        //             state.droneState = DECELERATING;
        //             break;
        //         }

        //         state.droneVelocity += acceleration * inputData.simTimeStep;
        //         if (state.droneVelocity >= inputData.attackSpeed)
        //         {
        //             state.droneVelocity = inputData.attackSpeed;
        //             state.droneState = MOVING;
        //         }
        //         break;
        //     }
        //     case DECELERATING:
        //     {
        //         LOG_INFO("Action STATE: DECELERATING");

        //         state.droneVelocity -= acceleration * inputData.simTimeStep;
        //         if (state.droneVelocity <= 0.0f)
        //         {
        //             state.droneVelocity = 0.0f;
        //             state.droneState = TURNING;
        //         }
        //         break;
        //     }
        //     default:
        //     {
        //         // ...
        //     }
        // }