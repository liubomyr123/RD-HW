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

/**
 * Написати консольну C++ програму, яка розраховує точку скиду боєприпасу з дрона на задану наземну ціль. 
 * Програма враховує: 
 *      - тип боєприпасу, 
 *      - його фізичні параметри (масу, drag-коефіцієнт, підйомну силу планерування), 
 *      - швидкість дрона
 *      - розгінну дистанцію.
 * 
 * Програма виводить у output.txt:
 *      - Проміжну точку маневру, якщо h + accelerationPath > D
 *      - Координати точки скиду (fireX, fireY)
 */

/**
 *      | Параметр          | Тип    | Опис                                   |
 *      -----------------------------------------------------------------------
 *      | xd, yd, zd        | float  | Координати дрона (zd — висота, м)      |
 *      | targetX, targetY  | float  | Координати цілі на землі (z = 0)       |
 *      | attackSpeed       | float  | Швидкість атаки дрона (м/с)            |
 *      | accelerationPath  | float  | Довжина розгону дрона перед скидом (м) |
 *      | ammo_name         | char[] | Назва боєприпасу                       |
 * 
 *      Приклад вхідного файлу: 
 *          100 100 100 200 200 10 10 VOG-17 
 *      Приклад вихідного файлу: 
 *          173.759 173.759
 */

struct InputData {
    float xd; 
    float yd; 
    float zd;
    float targetX; 
    float targetY;
    float attackSpeed;
    float accelerationPath;
    std::string ammo_name;
};

struct OutputData {
    float fireX;
    float fireY;
    float postManeuverX;
    float postManeuverY;
    bool isRecalculated;
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
    
    // Example: "100 100 100 200 200 10 10 VOG-17"
    file >> inputData.xd >> inputData.yd >> inputData.zd 
        >> inputData.targetX >> inputData.targetY 
        >> inputData.attackSpeed 
        >> inputData.accelerationPath 
        >> inputData.ammo_name;

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
    LOG_INFO("  - targetX: " << inputData.targetX);
    LOG_INFO("  - targetY: " << inputData.targetY);
    LOG_INFO("  - attackSpeed: " << inputData.attackSpeed);
    LOG_INFO("  - accelerationPath: " << inputData.accelerationPath);
    LOG_INFO("  - ammo_name: " << inputData.ammo_name);

    std::string extra;
    if (file >> extra) {
        LOG_WARN("Found extra data: " + extra + " (ignored)");
    }

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

bool getAmmoTimeOfFlight(float& result, const InputData& inputData, const AmmoInfo& outAmmo)
{
    LOG_PROCESS("Calculating time of fly...");

    float d = outAmmo.d;
    float m = outAmmo.m;
    float l = outAmmo.l;
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
    LOG_SUCCESS("Successfully found time of flight");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - t: " << t);

    result = t;
    return true;
}

bool getHorizontalFlightRange(float& result, const InputData& inputData, const AmmoInfo& outAmmo, const float& ammoTimeOfFlight)
{
    LOG_PROCESS("Calculating horizontal flight range...");

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
    float d = outAmmo.d;
    float m = outAmmo.m;
    if (std::abs(m) < 1e-6f)
    {
        LOG_ERROR("Invalid m: we cannot divide by zero");
        return false;
    }
    float l = outAmmo.l;
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

    LOG_SUCCESS("Successfully calculated horizontal flight range");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - h: " << h);

    result = h;
    return true;
}

bool getDistanceToTarget(float& result, const InputData& inputData)
{
    LOG_PROCESS("Calculating horizontal distance from copter to target...");

    // D = √( (targetX − xd)² + (targetY − yd)² )
    // targetX, targetY - координати цілі
    // xd, yd - координати дрона

    float xDiff = inputData.targetX - inputData.xd;
    float yDiff = inputData.targetY - inputData.yd;

    float step0 = xDiff * xDiff;
    float step1 = yDiff * yDiff;

    float D = std::sqrt(step0 + step1);

    LOG_SUCCESS("Successfully calculated distance from drone to target");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - D: " << D);

    result = D;
    return true;
}

bool isManeuverRequired(const float& h, const float& accelerationPath, const float& D)
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

bool getNewDroneCoordinatesForManeuver(float& newX, float& newY, const InputData& inputData, const float& D, const float& h)
{
    LOG_PROCESS("Calculating new drone coordinates for maneuver...");    

    // xd' = targetX − (targetX − xd) · (h + accelerationPath) / D
    // yd' = targetY − (targetY − yd) · (h + accelerationPath) / D

    if (std::abs(D) < 1e-6f)
    {
        LOG_ERROR("Invalid D: we cannot divide by zero");
        return false;
    }

    float step0 = (h + inputData.accelerationPath) / D;

    newX = inputData.targetX - (inputData.targetX - inputData.xd) * step0;
    newY = inputData.targetY - (inputData.targetY - inputData.yd) * step0;

    LOG_SUCCESS("Successfully calculated new drone coordinates for maneuver.");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - xd': " << newX);
    LOG_INFO("  - yd': " << newY);

    return true;
}

bool getAmmoDropPoint(OutputData& outputData, const InputData& inputData, const AmmoInfo& outAmmo, const float& D, const float& h)
{
    LOG_PROCESS("Calculating ammo drop point...");    

    // ratio = (D − h) / D
    // fireX = xd + (targetX − xd) · ratio
    // fireY = yd + (targetY − yd) · ratio

    if (std::abs(D) < 1e-6f)
    {
        LOG_ERROR("Invalid D: we cannot divide by zero");
        return false;
    }

    float newXd = inputData.xd;
    float newYd = inputData.yd;
    float newH = h;
    float newD = D;
    auto newInputData = inputData;
    if (isManeuverRequired(h, inputData.accelerationPath, D))
    {
        if (!getNewDroneCoordinatesForManeuver(newXd, newYd, inputData, D, h)) 
        {
            return false;
        }

        outputData.postManeuverX = newXd;
        outputData.postManeuverY = newYd;
        outputData.isRecalculated = true;

        newInputData.xd = newXd;
        newInputData.yd = newYd;

        if (!getDistanceToTarget(newD, newInputData)) 
        {
            return false;
        }
        if (std::abs(newD) < 1e-6f)
        {
            LOG_ERROR("Invalid D: we cannot divide by zero");
            return false;
        }

        float ammoTimeOfFlight = 0.0f;
        if (!getAmmoTimeOfFlight(ammoTimeOfFlight, newInputData, outAmmo)) 
        {
            return false;
        }

        if (!getHorizontalFlightRange(newH, newInputData, outAmmo, ammoTimeOfFlight)) 
        {
            return false;
        }
    }

    float ratio = (newD - newH) / newD;

    outputData.fireX = newInputData.xd + (newInputData.targetX - newInputData.xd) * ratio;
    outputData.fireY = newInputData.yd + (newInputData.targetY - newInputData.yd) * ratio;

    LOG_SUCCESS("Successfully calculated ammo drop point.");

    LOG_INFO("📄 Result:");
    LOG_INFO("  - fireX: " << outputData.fireX);
    LOG_INFO("  - fireY: " << outputData.fireY);

    return true;
}

bool writeOutputToFile(const std::string& file_name, const OutputData& outputData)
{
    LOG_PROCESS("Writing result to " << file_name << "...");

    std::ofstream file(file_name);
    if (!file)
    {
        LOG_ERROR("Error opening file");
        return false;
    }

    if (outputData.isRecalculated)
    {
        file << outputData.postManeuverX << " " << outputData.postManeuverY << " ";
    }
    file << outputData.fireX << " " << outputData.fireY;

    if (file.fail())
    {
        LOG_ERROR("Failed while writing to file");
        return false;
    }

    file.close();

    LOG_SUCCESS("Successfully wrote result to file");
    return true;
}

int main()
{
    std::string input_file_name = "input.txt";
    std::string output_file_name = "output.txt";

    InputData inputData{};
    if (!getInputData(input_file_name, inputData))
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

    float distanceToTarget = 0.0f;
    if (!getDistanceToTarget(distanceToTarget, inputData)) 
    {
        return 1;
    }

    OutputData outputData{};
    if (!getAmmoDropPoint(outputData, inputData, ammoInfo, distanceToTarget, horizontalFlightRange)) 
    {
        return 1;
    }

    if (!writeOutputToFile(output_file_name, outputData)) 
    {
        return 1;
    }

    return 0;
}


 /**
 * Step 1: Обчислити час польоту снаряда t
 * Ми маємо координати дрона коли він не рухається, ми знаємо відстань протягом якої він набере швидкість від 0 для атаки.
 * Ми також маємо тип снаряду і його параметри: масу (m), опір повітря (d) та коефіцієнт підйомної сили (l)
 * Тому на цьому кроці ми шукаємо скільки часу снаряд буде летіти, з певної висоти (zd - висота на якій знаходиться дрон в момент скиду снаряду)
 * маючи його початкову вертикальну швидкість (0 m/s в момент скиду) та горизонтальну швидкість (attackSpeed m/s - швидкість дрона в момент скиду)
 * Важливо: цей час t - це час від моменту відділення снаряда від дрона до моменту його падіння на землю.
 * 
 * Step 2: Обчислити горизонтальну дальність польоту h
 * Наступне що ми можемо зробити - знайти яку горизонтальну відстань снаряд пролетить протягом часу t.
 * Вертикально він пролетить відстань від висоти дрона (zd) до землі (0), але це вже враховано в t.
 * Тут нас цікавить тільки горизонтальний рух.
 * 
 * Step 3: Обчислити відстань від дрона до цілі D
 * На цьому кроці ми знаходимо яка відстань від дрона до цілі. Це горизонтальна пряма між 2 точками, не по діагоналі.
 * Тобто в декартовій системі координат (x та y). Тобто відстань між координатами x та y дрона (xd, yd) та цілі (targetX, targetY).
 * 
 * Step 4: Перевірити необхідність маневру 
 * Тепер важливо перевірити чи на достатній відстані дрон зараз знаходиться від цілі, ще до скиду.
 * Тобто якщо ми будемо занадто близько, тоді снаряд може перелетіти або не встигнути правильно лягти в траєкторію.
 * Для цього ми перевіряємо умову:
 *      (горизонтальна дальність польоту снаряда + відстань для розгону дрона) > відстані від дрона до цілі
 *              або
 *      (h + accelerationPath) > D
 * Якщо дрон заблизько - значить йому потрібно спочатку відійти/перелетіти на правильну дистанцію 
 * перед виконанням скиду та перерахувати усе з нивими координатами
 * 
 * Step 5: Знайти точку скиду
 * Це останній крок. Ми знаходимо ті координати для дрона, в яких він має скинути снаряд.
 * Отже, снаряд після скиду пролетить горизонтальну відстань h, тому ми повинні знайти точку,
 * яка знаходиться на відстані h перед ціллю вздовж напрямку "дрон → ціль".
 * Тобто ми рухаємось назад по прямій лінії між дроном і ціллю.
 */

 