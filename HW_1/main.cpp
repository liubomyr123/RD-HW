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

bool getAmmoInfoByType(const std::string ammo_name, AmmoInfo& outAmmo)
{
    auto it = ammo_types_info.find(ammo_name);
    
    if (it == ammo_types_info.end()) {
        LOG_WARN("Unknown ammo type: " << ammo_name);
        return false;
    }

    LOG_SUCCESS("Successfully found ammo type.");
    outAmmo = it->second;

    std::cout << "📄 Result:\n";
    std::cout << "  - m: " << outAmmo.m << "\n";
    std::cout << "  - d: " << outAmmo.d << "\n";
    std::cout << "  - l: " << outAmmo.l << "\n";
    std::cout << "  - isFreeFall: " << (outAmmo.isFreeFall ? "true" : "false") << "\n";

    return true;
}

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

    std::cout << "📄 Result:\n";
    std::cout << "  - xd: " << inputData.xd << "\n";
    std::cout << "  - yd: " << inputData.yd << "\n";
    std::cout << "  - zd: " << inputData.zd << "\n";
    std::cout << "  - targetX: " << inputData.targetX << "\n";
    std::cout << "  - targetY: " << inputData.targetY << "\n";
    std::cout << "  - attackSpeed: " << inputData.attackSpeed << "\n";
    std::cout << "  - accelerationPath: " << inputData.accelerationPath << "\n";
    std::cout << "  - ammo_name: " << inputData.ammo_name << "\n";

    std::string extra;
    if (file >> extra) {
        LOG_WARN("Found extra data: " + extra + " (ignored)");
    }

    return true;
}

int main()
{
    std::string file_name = "input.txt";

    InputData inputData{};
    if (!getInputData(file_name, inputData))
    {
        return 1;
    }

    AmmoInfo ammoInfo;
    if (!getAmmoInfoByType(inputData.ammo_name, ammoInfo)) 
    {
        return 1;
    }

    return 0;
}