#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>

enum DroneState
{
    STOPPED = 0,
    ACCELERATING,
    DECELERATING,
    TURNING,
    MOVING,
};

const int BOMB_COUNT = 5;
char bombNames [BOMB_COUNT][15] = {"VOG-17", "M67", "RKG-3", "GLIDING-VOG", "GLIDING-RKG"};
float bombM[BOMB_COUNT]         = {0.35f,   0.6f,   1.2f,   0.45f,  1.4f};
float bombD[BOMB_COUNT]         = {0.07f,   0.1f,   0.1f,   0.1f,   0.1f};
float bombL[BOMB_COUNT]         = {0.0f,    0.0f,   0.0f,   1.0f,   1.0f};

const float g_gravity = 9.81f;
const int MAX_STEPS = 10000;

float targetXInTime[5][60];
float targetYInTime[5][60];

float   outX[MAX_STEPS + 1];
float   outY[MAX_STEPS + 1];
float   outDir[MAX_STEPS + 1];
int     outState[MAX_STEPS + 1];
int     outTarget[MAX_STEPS + 1];

void interpolateTarget(int targetIdx, float t, float arrayTimeStep,
                        float& outTx, float& outTy)
{
    int idx = (int)std::floor(t/arrayTimeStep) % 60;
    int next = (idx + 1) % 60;
    float frac = (t/arrayTimeStep) - std::floor(t/arrayTimeStep);

    outTx = targetXInTime[targetIdx][idx]
        + (targetXInTime[targetIdx][next] - targetXInTime[targetIdx][idx]) * frac;

    outTy = targetYInTime[targetIdx][idx]
        + (targetYInTime[targetIdx][next] - targetYInTime[targetIdx][idx]) * frac;
}

float normalizeAngle(float a)
{
    while (a > M_PI) a -= 2.0f * (float)M_PI;
    while (a < -M_PI) a += 2.0f * (float)M_PI;
    return a;
}

float calcTimeOfFlight(float Z0, float V0, float m, float d, float l)
{
    float a = d * g_gravity * m - 2 * d * d * l * V0;
    float b = -3 * g_gravity * m * m + 3 * d * l * m * V0;
    float c = 6 * m *m * Z0;

    if (std::fabs(a) < 1e-12f)
    {
        return std::sqrt(2.0f * Z0 / g_gravity);
    }

    float p = -b * b / (3 * a * a);
    float q = (2 * b * b * b) / (27 * a * a * a) + c / a;

    if (p >= 0)
    {
        return std::sqrt(2.0f * Z0 / g_gravity);
    }

    float arg = 3 * q / (2 * p) * std::sqrt(-3 / p);

    if (std::fabs(arg) > 1)
    {
        return std::sqrt(2.0f * Z0 / g_gravity);
    }

    float phi = std::acos(arg);
    float t = 2 * std::sqrt(-p / 3) * std::cos((phi + 4 * (float)M_PI) / 3) - b / (3 * a);

    return t > 0 ? t : std::sqrt(2.0f * Z0 / g_gravity);
}

float calcHDistance(float t, float v0, float m, float d, float l)
{
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
    float step2 = t3 * ((6 * d * g_gravity * l * m) - (6 * d2 * (l2 - 1) * v0)) / (36 * m2);

    // −6d²g·l·(1+l²+l⁴)m
    float step3_0 = (-1) * 6 * d2 * g_gravity * l * (1 + l2 + l4) * m;
    // + 3d³l²(1+l²)V₀
    float step3_1 = 3 * d3 * l2 * (1 + l2) * v0;
    // + 6d³l⁴(1+l²)V₀
    float step3_2 = 6 * d3 * l4 * (1 + l2) * v0;
    // 36(1+l²)²m³
    float step3_3 = 36 * (1 + l2) * (1 + l2) * m3;

    float step3 = t4 * (step3_0 + step3_1 + step3_2) / step3_3;

    // 3d³g·l³m
    float step4_0 = 3 * d3 * g_gravity * l3 * m;
    // − 3d⁴l²(1+l²)V₀
    float step4_1 = (-1) * 3 * d4 * l2 * (1 + l2)* v0;
    // 36(1+l²)m⁴
    float step4_2 = 36 * (1 + l2) * m4;

    float step4 = t5 * (step4_0 + step4_1) / step4_2;

    float h = step0 + step1 + step2 + step3 + step4;

    return h;
}


int main()
{
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
    char ammo_name[15] = "";

    {
        std::ifstream file("input.txt");
        if (!file.is_open())
        {
            std::cerr << "Can not open input.txt" << std::endl;
            return 1;
        }

        file >> xd >> yd >> zd 
            >> initialDir
            >> attackSpeed 
            >> accelerationPath 
            >> ammo_name
            >> arrayTimeStep
            >> simTimeStep
            >> hitRadius
            >> angularSpeed
            >> turnThreshold;

        file.close();
    }

    int bombIdx = -1;
    for (size_t i = 0; i < BOMB_COUNT; i++)
    {
        if (std::strcmp(ammo_name, bombNames[i]) == 0)
        {
            bombIdx = i;
            break;
        }
    }
    if (bombIdx < 0)
    {
        std::cerr << "Unknown ammo" << std::endl;
        return 1;
    }

    float m = bombM[bombIdx];
    float d = bombD[bombIdx];
    float l = bombL[bombIdx];

    {
        std::ifstream file("targets.txt");
        if (!file.is_open())
        {
            std::cerr << "Can not open targets.txt" << std::endl;
            return 1;
        }
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 60; j++)
            {
                file >> targetXInTime[i][j];
            }
        }
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 60; j++)
            {
                file >> targetYInTime[i][j];
            }
        }

        file.close();
    }

    float acceleration = attackSpeed * attackSpeed / (2.0f * accelerationPath);

    float droneX = xd;
    float droneY = yd;
    float direction = initialDir;
    float speed = 0.0f;
    DroneState state = STOPPED;

    float currentTime = 0.0f;
    int currentTarget = -1;
    float turnRemaining = 0.0f;
    float targetDir = initialDir;

    int step = 0;

    float flightTime = calcTimeOfFlight(zd, attackSpeed, m, d, l);
    float hDist = calcHDistance(flightTime, attackSpeed, m, d, l);

    while(step <= MAX_STEPS)
    {
        // 1
        float tgtX[5], tgtY[5];
        for (int i = 0; i < 5; i++)
        {
            interpolateTarget(i, currentTime, arrayTimeStep, tgtX[i], tgtY[i]);
        }
        
        // 2
        float bestTime = 1e9f; // оцінка повного часу до ураження
        int bestTarget = 0;
        float bestPredX = droneX;
        float bestPredY = droneY;

        float bestFireX = droneX;
        float bestFireY = droneY;

        for (int i = 0; i < 5; i++)
        {
            float predX, predY;
            // Шукаємо де буде ціль через час польоту снаряду.
            // Тобто це той мінімальний час який нам треба щоб попасти в ціль якби ми уже були в точці скиду
            // Ми завжди беремо currentTime для того щоб робити розрахунки з актуальними даними, не застарілими
            // і щоб прогноз був робочим, треба усе розраховувати від точки currentTime 
            interpolateTarget(i, currentTime + flightTime, arrayTimeStep, predX, predY);

            // Знаходимо перше грубе припущення що коорднита де ми будемо скидувати снаряд
            // Спочатку вважаємо, що скидання відбудеться прямо в поточну прогнозовану позицію цілі
            float fx = predX;
            float fy = predY;

            for (int inter = 0; inter < 6; inter++)
            {
                // Знаходимо вектор від дрона до прогнозованої позиції цілі
                // Бо вектор між 2 точками ми отримуємо при відмінанні цих 2 точок
                // 
                // Наприклад: 
                // Дрон (2, 3)
                // Ціль (7, 11)
                // 
                // Тоді:
                //      dxT = 7 - 2 = 5
                //      dyT = 11 - 3 = 8
                // Тобто вектор від дрона до цілі (5; 8)
                float dxT = predX - droneX;
                float dyT = predY - droneY;

                // Далі знаходимо відстань по цьому вектору, тобто відстань від дрона до прогнозованої позиції цілі
                float distT = std::hypot(dxT, dyT); // sqrt(x*x + y*y)
                if (distT < 1e-6f) // якщо значення близьке до 0, ми не хочемо ділити на 0, тому ставимо самі дуже мале число 0.0...01
                {
                    distT = 1e-6f;
                }
                // dxT / distT та dyT / distT - це одиничні вектори, тобто вектори які мають довжину 1
                // Основне завдання одиничних векторів - показати напрямок, а далі ми можемо вказати на яку вістань
                // Ось тут: 
                //      dxT / distT * hDist - це прокласти вектор довжиною hDist в напрямку одиничного вектора (dxT / distT)
                // Так само і для Y
                // Після того як ми знайшли вектор довжиною hDist, тобто ту відстань яку буде летіти снаряд
                // Ми знаходимо новий вектор від цілі (predX; predY) мінус відстань яку буде летіти снаряд
                // Таким чином ми знаходимо точку скиду снаряду
                fx = predX - dxT / distT * hDist;
                fy = predY - dyT / distT * hDist;

                // Тепер коли ми знаємо координати скиду, можемо розрахувати відстань від дрона до точки скиду
                float distToFire = std::hypot(fx - droneX, fy - droneY);
                // Розберемо тепер tImpact
                // Маємо distToFire / attackSpeed - це класичне знаходження часу,
                // маючи відстань яку треба пролетіти та швидкість дрона, тільки ми робемо перевірку attackSpeed щоб не ділити на 0
                // І в кінці ми додаємо час польоту снаряду
                // ТАким чином ми отримуємо час на ураження = скільки буде летіти дрон до точки скиду + час падіння снаряду
                float tImpact = distToFire / (attackSpeed > 0.1f ? attackSpeed : 0.1f) + flightTime;
                // І нарешті, ми шукаємо де буде знаходитися ціль через tImpact час і оновлюємо прогнозовані координати predX та predY
                interpolateTarget(i, currentTime + tImpact, arrayTimeStep, predX, predY);
            }

            // Знову знаходимо відстань від дрона до точки скиду снаряду
            float distToFire = std::hypot(fx - droneX, fy - droneY);
            // І відстань від дрона до прогнозованої позиції цілі
            float distToTgt = std::hypot(predX - droneX, predY - droneY);

            float totalTime;
            if (distToTgt < hDist) // Якщо ціль занадто близько, нам треба зробити маневр
            {
                // Розберемо цей вираз.
                // Почнемо з (2 * hDist - distToTgt) - це правило, яке використовується,
                // коли дрон вже знаходиться занадто близько до цілі відносно оптимальної дистанції hDist.
                // Ми беремо 2 * hDist, бо зараз ми уже distToTgt < hDist. Це означає що нам треба як мінімум 
                // відлетіти назад ше на відстань hDist - distToTgt. Це та відстань де ми реально можемо скинути снаряд, 
                // але для загального правила беремо 2 * hDist. Тому ми беремо цю нову штрафну відстань (2 * hDist - distToTgt) 
                // Далі просто - беремо цю відстань і знаходимо скільки часу треба летіти цю відстань на швидкості attackSpeed
                // 
                // Потім ми беремо angularSpeed - це макс швидкість з якою дрон може 
                // розвертатися - рад/с - або скільки радіан за секунду
                // І оцінюємо час на великий маневр, який у гіршому випадку еквівалентний розвороту на 180 градуів
                // 
                // 1 радіан = (180 градусів / M_PI)
                // 1 радіан * M_PI = 180 градусів
                // M_PI = 180 градусів / 1 радіан
                // 
                // Нехай у нас angularSpeed = 2 рад/с, тобто за 1 секунду ми повертаємось на 2 радіани або 2 * (180 градусів / M_PI)
                // Тоді щоб знайти час скільки дрон буде розвертатися на 180 градусів:
                // 1 сек - 2 * (180 градусів / M_PI)
                // х сек - 180 градусів
                // 
                // x сек = (180 градусів * 1 сек) / (2 * (180 градусів / M_PI))
                // x сек = ((180 градусів * 1 сек) / 1) * (M_PI / (2 * 180 градусів))
                // x сек = ((1 сек) / 1) * (M_PI / (2))
                // x сек = M_PI / 2
                // 
                // І далі ми додаємо flightTime і знаходимо оцінка повного часу ураження цілі, що складається з 
                // часу маневрування дрона для виходу в позицію атаки, 
                // часу розвороту (обмеженого кутовою швидкістю) та часу польоту снаряда
                totalTime = (2 * hDist - distToTgt) / attackSpeed + (float)M_PI / angularSpeed + flightTime;
            }
            else // Ціль достатньо далеко, ми просто знаходимо час долетіти до точки скиду + час снаряду
            {
                totalTime = distToFire / attackSpeed + flightTime;
            }

            // Перед тим як оновити повний час на долітання та скидання,
            // перевіряємо чи поточна ціль яку ми перевіряємо є ціль до якої ми уже рухаємося
            // Якщо це та сама ціль, ми пропускаємо її, бо ми уже врахували для неї усе на поперньому кроці симуляції
            // Але якщо це інша ціль, тобто в процесі польоту ми побачили що є інша ближча ціль, ми 
            // хочемо оновити цей час залежно від того в якому стані дрон
            // switch(state) оцінює, скільки часу дрон втратить, якщо зараз змінить напрям руху, 
            // залежно від того, наскільки він вже у поточному русі
            // Для чого це нам???????????
            if (i != currentTarget && currentTarget >= 0)
            {
                switch (state)
                {
                    case STOPPED:
                        break;
                        // speed - це швидкість яку набрав дрон на поточний момент часу
                        // acceleration - це стабільне прискорення
                        // speed / acceleration - повертає нам час який ми потратили для того щоб набрати цю швидкість
                        // Тому ми хочемо врахувати цей час якщо наприклад інша ціль є ближче, 
                        // нам треба знати скільки часу ми потенційно потратимо на зміну цілі, тобто певний час по енерції
                    case ACCELERATING:
                        // v / a — це час, який потрібен щоб розігнатися до v, і симетрично це час 
                        // щоб повністю втратити цю швидкість при гальмуванні
                        totalTime += speed / acceleration;
                        break;
                    case MOVING:
                        totalTime += attackSpeed / acceleration;
                        break;
                    case TURNING:
                        // якщо дрон уже розвертається, то беремо скільки часу він тратить на кожний розворот
                        totalTime += turnRemaining;
                        break;
                    case DECELERATING:
                        totalTime += speed / acceleration;
                        break;
                    default:
                        break;
                    }
            }

            // Перевіряємо чи ми знайшли нову ціль з кращим часом ураження враховуючи інерцію дрона
            if (totalTime < bestTime)
            {
                bestTime = totalTime;
                bestTarget = i;
                bestPredX = predX;
                bestPredY = predY;
                bestFireX = fx;
                bestFireY = fy;
            }
        }
        
        currentTarget = bestTarget;

        // 3
        // Знову знаходимо відстань від поточного положення дрона до координат цілі
        float dxT = bestPredX - droneX;
        float dyT = bestPredY - droneY;
        float distT = std::hypot(dxT, dyT);
        if (distT < 1e-6f)
        {
            distT = 1e-6f;
        }

        float fireX, fireY;

        // Знову перевіряємо чи дрон на достатній відстані до цілі
        // Якщо заблизько до цілі по відстані ураження снаряду, будемо маневрувати 
        // І шукаємо точку скиду для вибраної найкращої цілі
        if (distT >= hDist)
        {
            fireX = bestFireX;
            fireY = bestFireY;
        }
        else
        {
            fireX = droneX - dxT / distT * hDist;
            fireY = droneY - dyT / distT * hDist;
        }

        // Знаходимо вектор від дрона до точки скиду
        float dx = fireX - droneX;
        float dy = fireY - droneY;

        // І знаходимо кут між на який дрон має повернутися щоб дивитися на точку скиду
        float desiredDir = std::atan2(dy, dx);
        // direction - куди зараз дивимться дрон, desiredDir - куди має дивитися, на точку скиду
        // deltaAngle - показує наскільки треба повернутися щоб почати дивитися в точку скиду
        float deltaAngle = normalizeAngle(desiredDir - direction);

        // 4
        outX[step] = droneX;
        outY[step] = droneY;
        outDir[step] = direction;
        outState[step] = (int)state;
        outTarget[step] = currentTarget;

        // 5
        // Тут ми шукаємо координати точки яка буде перед дроном в напрямку direction через відстань в hDist
        // Іншими словами ми рахуємо де впаде снаряд зараз якщо її скинути зараз з місця 
        // де є дрон в напрямку самого дрона, бо direction - це куди зараз дивиться дрон
        float bombX = droneX + std::cos(direction) * hDist;
        float bombY = droneY + std::sin(direction) * hDist;

        float predTgtX, predTgtY;
        // Просто шукаємо де буде вибрана ціль через час польоту снаряду, тобто коли снаряд долетить до землі
        interpolateTarget(currentTarget, currentTime + flightTime, arrayTimeStep, predTgtX, predTgtY);

        // Тут шукаємо відстань між місцем де буде ціль в момент падіння снаряду і місцем де впаде снаряд
        float missIfFired = std::hypot(bombX - predTgtX, bombY - predTgtY);
        // speed - це швидкість з якою дрон рухається прямо зараз
        // simTimeStep - це крок симуляції
        // Тоді їх добуток - це буде яку відстань долає дрон за 1 крок симуляції з певною швидкістю
        float dropPrecision = speed * simTimeStep;
        if (dropPrecision < 0.01f)
        {
            dropPrecision = 0.01f;
        }
        // І тепер, якщо дрон рухається і якщо відстань від місця де впаде снаряд і де буде ціль є менша за 
        // той крок який долає за фрейм симуляції - значить ми скидаємо наш снаряд
        if (missIfFired < dropPrecision && state == MOVING)
        {
            break;
        }

        // 6
        switch (state)
        {
            case STOPPED:
            {
                if (std::fabs(deltaAngle) > turnThreshold)
                {
                    state = TURNING;
                    turnRemaining = std::fabs(deltaAngle) / angularSpeed;
                    targetDir = desiredDir;
                }
                else
                {
                    direction = desiredDir;
                    state = ACCELERATING;
                }
                break;
            }
            case ACCELERATING:
            {
                if (std::fabs(deltaAngle) > turnThreshold && speed > 0.01f)
                {
                    state = DECELERATING;
                }
                else
                {
                    speed += acceleration * simTimeStep;
                    if (speed >= attackSpeed)
                    {
                        speed = attackSpeed;
                        state = MOVING;
                    }
                    if (std::fabs(deltaAngle) <= turnThreshold)
                    {
                        direction = desiredDir;
                    }

                    droneX += std::cos(direction) * speed * simTimeStep;
                    droneY += std::sin(direction) * speed * simTimeStep;
                }
                break;
            }
            case DECELERATING:
            {
                speed -= acceleration * simTimeStep;
                if (speed <= 0.0f)
                {
                    speed = 0.0f;
                    state = STOPPED;
                }
                droneX += std::cos(direction) * speed * simTimeStep;
                droneY += std::sin(direction) * speed * simTimeStep;
                break;
            }
            case TURNING:
            {
                float turnStep = angularSpeed * simTimeStep
                    * (normalizeAngle(targetDir - direction) >= 0 ? 1.0f : -1.0f);
                direction = normalizeAngle(direction + turnStep);
                turnRemaining -= simTimeStep;
                
                if (turnRemaining <= 0.0f)
                {
                    direction = targetDir;
                    state = ACCELERATING;
                    turnRemaining = 0.0f;
                }
                break;
            }  
            case MOVING:
            {
                if (std::fabs(deltaAngle) > turnThreshold)
                {
                    state = DECELERATING;
                }
                else
                {
                    if (std::fabs(deltaAngle) <= turnThreshold)
                    {
                        direction = desiredDir;
                    }

                    droneX += std::cos(direction) * speed * simTimeStep;
                    droneY += std::sin(direction) * speed * simTimeStep;
                }
                break;
            }              
            default:
            {
                // ...
                break;
            }
        }

        currentTime += simTimeStep;
        step++;
    }

    // 7
    int N = step;

    std::ofstream fileOut("simulation.txt");
    if (!fileOut.is_open())
    {
        std::cerr << "Can not write to out file" << std::endl;
        return 1;
    }

    fileOut << N << std::endl;

    for (int i = 0; i < N; i++)
    {
        if (i > 0)
        {
            fileOut << " ";
        }
        fileOut << outX[i] << " " << outY[i];
    }
    fileOut << std::endl;
    
    for (int i = 0; i < N; i++)
    {
        if (i > 0)
        {
            fileOut << " ";
        }
        fileOut << outDir[i];
    }
    fileOut << std::endl;

    for (int i = 0; i < N; i++)
    {
        if (i > 0)
        {
            fileOut << " ";
        }
        fileOut << outState[i];
    }
    fileOut << std::endl;

    for (int i = 0; i < N; i++)
    {
        if (i > 0)
        {
            fileOut << " ";
        }
        fileOut << outTarget[i];
    }
    fileOut << std::endl;

    fileOut.close();

    return 0;
}