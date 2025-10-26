/*
==========================================================
  Projeto : Controle_Poco_Com_Autocalibracao_Filter
  Autor   : Paulo Francisco Carvalho Araújo
  Data    : 26/10/2025
  Versão  : 2.0
==========================================================
🧩 Descrição:
  Sistema de controle de bomba para poço artesiano com:
   - Leitura de dois sensores ZMPT101B (nível mínimo e máximo)
   - Autocalibração automática ao iniciar (mantendo botão pressionado por 2s)
   - Botão adicional para calibração manual em operação
   - Filtro exponencial (alpha = 0.2) para suavizar leituras RMS
   - Identificação de estados do poço (vazio, enchendo, cheio, descendo, falha)
   - Controle automático da bomba conforme os níveis detectados
   - Saída serial compatível com Serial Plotter para visualização dinâmica

⚙️ Recursos implementados:
   • Modo Automático com LED indicador
   • Debounce nos botões
   • Função de calibração automática e manual
   • Funções de leitura RMS com offset dinâmico
   • Proteção contra falha de sensores

🔌 Mapeamento de Pinos:
   ├─ Botão modo automático ......... D7
   ├─ Botão calibração manual ........ D6
   ├─ LED de modo automático ......... D12
   ├─ Saída de controle da bomba ..... D8
   ├─ Relé/Bomba duplicado ........... D11
   ├─ Sensor nível mínimo (ZMPT1) .... A1
   ├─ Sensor nível máximo (ZMPT2) .... A2

💡 Observações:
   • Para iniciar autocalibração automática, mantenha o botão de modo pressionado
     durante os primeiros 2 segundos após ligar o sistema.
   • A calibração manual pode ser feita a qualquer momento com o botão dedicado.
   • Mantenha os sensores imersos durante a calibração.

==========================================================
*/

#include <math.h>

#define botao 7
#define botaoCalib 6
#define bombaPin 8
#define ledModo 12
#define Bomba 11
#define sensorMin A1
#define sensorMax A2

// ============================
// 🔧 Configurações e variáveis
// ============================
bool estadoBotaoEstavel = HIGH;
bool ultimoEstadoLido = HIGH;
unsigned long ultimoTempoTroca = 0;
const unsigned long tempoDebounce = 50;

bool estadoBotaoCalibEstavel = HIGH;
bool ultimoEstadoCalibLido = HIGH;
unsigned long ultimoTempoCalib = 0;

bool modoAutomatico = false;
bool bombaLigada = false;

// --- Estados do poço ---
enum EstadoPoco { POCO_CHEIO, POCO_DESCENDO, POCO_VAZIO, POCO_ENCHENDO, SENSOR_FALHA, FALHA_ELETRICA };
EstadoPoco estadoAtual = POCO_VAZIO;
EstadoPoco estadoAnterior = POCO_VAZIO;

// --- Calibração ---
float calibrationFactor = 242.0;
float ganhoMin = 1.00;
float ganhoMax = 1.00;

// --- Filtros ---
float filtroVmin = 0.0;
float filtroVmax = 0.0;
const float alpha = 0.2;  // 0.1 mais suave, 0.3 mais rápido

// --- Limiares ---
const float LIMITE_MOLHADO = 4.5;  // abaixo disso = molhado
const float LIMITE_SECO    = 8.0;  // acima disso = seco

unsigned long ultimaLeitura = 0;

// ============================
// 🔧 Funções auxiliares
// ============================
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

bool tempoDecorrido(unsigned long intervalo) {
  unsigned long agora = millis();
  if (agora - ultimaLeitura >= intervalo) {
    ultimaLeitura = agora;
    return true;
  }
  return false;
}

// ============================
// ⚙️ Funções de leitura
// ============================
float lerTensaoAC_RMS_comOffset(int pino) {
  const int amostras = 250;
  long somaLeit = 0;
  for (int i = 0; i < amostras; i++) somaLeit += analogRead(pino);
  float offset = somaLeit / (float)amostras;

  long somaQ = 0;
  for (int i = 0; i < amostras; i++) {
    int r = analogRead(pino);
    float c = r - offset;
    somaQ += (long)(c * c);
  }

  float rms_counts = sqrt(somaQ / (float)amostras);
  float tensaoSensor = (rms_counts * 2.5f) / 512.0f;
  return tensaoSensor * calibrationFactor;
}

float lerFiltradoComGanho(int pino, float &filtro, float ganhoCanal) {
  float v = lerTensaoAC_RMS_comOffset(pino) * ganhoCanal;
  filtro = alpha * v + (1 - alpha) * filtro;
  return filtro;
}

// ============================
// 🔧 Calibração Automática
// ============================
void autoCalibrar() {
  Serial.println("🛠 Iniciando autocalibração (mantenha os eletrodos IMERSOS)...");
  unsigned long t0 = millis();
  while (millis() - t0 < 2000) { /* aguardando 2s */ }

  float vmin = 0, vmax = 0;
  const int N = 8;
  for (int i = 0; i < N; i++) {
    vmin += lerTensaoAC_RMS_comOffset(sensorMin);
    vmax += lerTensaoAC_RMS_comOffset(sensorMax);
  }
  vmin /= N;
  vmax /= N;

  float alvo = (vmin + vmax) / 2.0f;
  if (vmin > 0.1f) ganhoMin = alvo / vmin;
  if (vmax > 0.1f) ganhoMax = alvo / vmax;

  Serial.print("✅ Autocalibrado. ganhoMin=");
  Serial.print(ganhoMin, 3);
  Serial.print(" ganhoMax=");
  Serial.println(ganhoMax, 3);
}

void autoCalibrarSeBotaoPressionado() {
  if (digitalRead(botao) == LOW) {
    autoCalibrar();
  }
}

// ============================
// 💧 Controle de bomba e estados
// ============================
void ligarBomba() {
  if (!bombaLigada) {
    bombaLigada = true;
    digitalWrite(bombaPin, HIGH);
    digitalWrite(Bomba, HIGH);
    Serial.println("✅ Bomba LIGADA");
  }
}

void desligarBomba() {
  if (bombaLigada) {
    bombaLigada = false;
    digitalWrite(bombaPin, LOW);
    digitalWrite(Bomba, LOW);
    Serial.println("⚠️ Bomba DESLIGADA");
  }
}

void atualizarEstado(float tensaoMin, float tensaoMax) {
  bool eminMolhado = tensaoMin <= LIMITE_MOLHADO;
  bool emaxMolhado = tensaoMax <= LIMITE_MOLHADO;

  Serial.print("Vmin: ");
  Serial.print(tensaoMin, 2);
  Serial.print(" V | Vmax: ");
  Serial.print(tensaoMax, 2);
  Serial.print(" V | ");

  estadoAnterior = estadoAtual;

  if (!eminMolhado && emaxMolhado) {
    estadoAtual = SENSOR_FALHA;
    Serial.println("🚨 Falha detectada: Sensor inferior com defeito!");
    return;
  }

  switch (estadoAtual) {
    case POCO_VAZIO:
      if (eminMolhado && emaxMolhado) {
        estadoAtual = POCO_CHEIO;
        Serial.println("💧 Poço cheio — pronto para bombear");
      } else if (eminMolhado && !emaxMolhado) {
        estadoAtual = POCO_ENCHENDO;
        Serial.println("🔄 Poço começando a encher...");
      } else {
        Serial.println("⚠️ Poço vazio — aguardando enchimento");
      }
      break;

    case POCO_ENCHENDO:
      if (eminMolhado && emaxMolhado) {
        estadoAtual = POCO_CHEIO;
        Serial.println("💧 Poço cheio — pronto para bombear");
      } else if (!eminMolhado && !emaxMolhado) {
        estadoAtual = POCO_VAZIO;
        Serial.println("⚠️ Poço ainda vazio");
      } else {
        Serial.println("⏳ Enchendo...");
      }
      break;

    case POCO_CHEIO:
      if (eminMolhado && !emaxMolhado) {
        estadoAtual = POCO_DESCENDO;
        Serial.println("⏬ Nível descendo...");
      } else if (!eminMolhado && !emaxMolhado) {
        estadoAtual = POCO_VAZIO;
        Serial.println("⚠️ Poço secou!");
      } else {
        Serial.println("💧 Mantendo estado: cheio");
      }
      break;

    case POCO_DESCENDO:
      if (!eminMolhado && !emaxMolhado) {
        estadoAtual = POCO_VAZIO;
        Serial.println("⚠️ Nível crítico — poço vazio");
      } else if (eminMolhado && emaxMolhado) {
        estadoAtual = POCO_CHEIO;
        Serial.println("💧 Poço reabastecido");
      } else {
        Serial.println("⏳ Poço ainda com água");
      }
      break;

    case SENSOR_FALHA:
      if (eminMolhado || !emaxMolhado) {
        Serial.println("✅ Falha resolvida — voltando à leitura normal");
        estadoAtual = POCO_VAZIO;
      } else {
        Serial.println("🚨 Aguardando correção do sensor inferior...");
      }
      break;
  }

  if (estadoAtual != estadoAnterior) {
    Serial.print("📊 Transição: ");
    Serial.print(estadoAnterior);
    Serial.print(" ➜ ");
    Serial.println(estadoAtual);
  }
}

void controlarBomba() {
  if (estadoAtual == FALHA_ELETRICA || estadoAtual == SENSOR_FALHA) {
    desligarBomba();
    return;
  }

  switch (estadoAtual) {
    case POCO_CHEIO:
    case POCO_DESCENDO:
      ligarBomba();
      break;
    case POCO_VAZIO:
    case POCO_ENCHENDO:
      desligarBomba();
      break;
    default:
      break;
  }
}

// ============================
// 🔘 Botões com debounce
// ============================
bool verificarBotao() {
  bool leituraAtual = digitalRead(botao);
  unsigned long agora = millis();
  if (leituraAtual != ultimoEstadoLido) {
    ultimoTempoTroca = agora;
    ultimoEstadoLido = leituraAtual;
  }
  if ((agora - ultimoTempoTroca) > tempoDebounce) {
    if (leituraAtual != estadoBotaoEstavel) {
      estadoBotaoEstavel = leituraAtual;
      return true;
    }
  }
  return false;
}

bool verificarBotaoCalib() {
  bool leituraAtual = digitalRead(botaoCalib);
  unsigned long agora = millis();
  if (leituraAtual != ultimoEstadoCalibLido) {
    ultimoTempoCalib = agora;
    ultimoEstadoCalibLido = leituraAtual;
  }
  if ((agora - ultimoTempoCalib) > tempoDebounce) {
    if (leituraAtual != estadoBotaoCalibEstavel) {
      estadoBotaoCalibEstavel = leituraAtual;
      return true;
    }
  }
  return false;
}

void limparSerial() {
  for (int i = 0; i < 40; i++) {
    Serial.println(); // Envia 40 linhas em branco
  }
}


// ============================
// 🧩 Setup e Loop principal
// ============================
void setup() {
  Serial.begin(9600);
  limparSerial();
  Serial.println("===============================================");
  Serial.println("===  Sistema de Controle de Poço Artesiano  ===");
  Serial.println("===============================================");
  Serial.println("💤 Sistema pronto — bomba desligada");
  Serial.println("🟢 Pressione o botão MODO para iniciar o sistema automático");
  Serial.println("⚙️  Ou mantenha pressionado por 2 segundos na inicialização");
  Serial.println("    para executar a calibração automática dos sensores (ZMPT).");
  Serial.println("---------------------------------------------------------------");

  pinMode(botao, INPUT_PULLUP);
  pinMode(botaoCalib, INPUT_PULLUP);
  pinMode(bombaPin, OUTPUT);
  pinMode(ledModo, OUTPUT);
  pinMode(Bomba, OUTPUT);

  digitalWrite(bombaPin, LOW);
  digitalWrite(ledModo, LOW);
  digitalWrite(Bomba, LOW);

  autoCalibrarSeBotaoPressionado();
}

void loop() {
  // alternar modo automático
  if (verificarBotao() && estadoBotaoEstavel == LOW) {
    modoAutomatico = !modoAutomatico;
    digitalWrite(ledModo, modoAutomatico ? HIGH : LOW);
    if (modoAutomatico)
      Serial.println("⚙️ Sistema ligado — modo automático");
    else {
      Serial.println("💤 Sistema pronto — bomba desligada");
      desligarBomba();
    }
  }

  // botão de calibração manual
  if (verificarBotaoCalib() && estadoBotaoCalibEstavel == LOW) {
    autoCalibrar();
  }

  if (modoAutomatico && tempoDecorrido(300)) {
    float tensaoMin = lerFiltradoComGanho(sensorMin, filtroVmin, ganhoMin);
    float tensaoMax = lerFiltradoComGanho(sensorMax, filtroVmax, ganhoMax);

    atualizarEstado(tensaoMin, tensaoMax);
    controlarBomba();

    // --- Envia dados formatados para o Serial Plotter ---
    Serial.print(filtroVmin, 2);
    Serial.print("\t");
    Serial.println(filtroVmax, 2);
  }
}
