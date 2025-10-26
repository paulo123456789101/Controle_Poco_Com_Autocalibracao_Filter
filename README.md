# 💧 Controle de Poço — Sistema Automático com Autocalibração e Filtro Dinâmico

## 🧠 Descrição do Projeto
Este projeto implementa um sistema inteligente de **controle automático de bomba para poço artesiano**, utilizando sensores de tensão **ZMPT101B** como eletrodos de nível (mínimo e máximo).  
O sistema identifica o estado do poço (cheio, enchendo, vazio ou descendo) e aciona a bomba de forma totalmente automática, garantindo segurança e eficiência.

Destaques:
- ⚙️ **Autocalibração automática** ao ligar (mantendo o botão pressionado por 2 s).  
- 🧩 **Filtro exponencial** para suavizar leituras RMS.  
- 💡 **Dois botões físicos:** modo automático e calibração manual.  
- 🚨 Detecção de **falhas em sensores** e proteção contra acionamento indevido da bomba.  
- 🔋 Totalmente **não bloqueante**, usando `millis()` para temporização.  
- 📈 Compatível com o **Serial Plotter** para visualização das tensões em tempo real.

---

## 🔧 Componentes Utilizados

| Componente              | Função                                       | Quantidade |
|--------------------------|----------------------------------------------|-------------|
| Arduino UNO (ou ESP32)   | Microcontrolador principal                   | 1 |
| Sensor ZMPT101B          | Sensor de tensão AC (nível mínimo/máximo)    | 2 |
| LED verde                | Indica bomba ligada                          | 1 |
| LED azul                 | Indica modo automático ativo                 | 1 |
| Botão (pushbutton)       | Alterna modo automático                      | 1 |
| Botão (pushbutton)       | Executa calibração manual                    | 1 |
| Relé 5 V / 10 A          | Acionamento da bomba                         | 1 |
| Resistores e jumpers     | Circuito de interface                        | Diversos |
| Fonte 5 VDC              | Alimentação do sistema                       | 1 |

---

## 🔌 Mapeamento de Pinos

| Função                          | Pino Arduino |
|----------------------------------|--------------|
| Botão modo automático            | D7 |
| Botão calibração manual          | D6 |
| LED modo automático              | D12 |
| Saída de controle da bomba       | D8 |
| Saída duplicada da bomba (relé)  | D11 |
| Sensor nível mínimo (ZMPT1)      | A1 |
| Sensor nível máximo (ZMPT2)      | A2 |

---

## 🧩 Algoritmo do Sistema

1. **Inicialização**
   - Configura os pinos de entrada/saída.  
   - Exibe informações iniciais no `Serial Monitor`.  
   - Se o botão de modo for mantido pressionado por 2 s, inicia a **autocalibração** automática.

2. **Leitura dos Sensores**
   - Mede o valor RMS de cada sensor ZMPT101B (250 amostras).  
   - Aplica **offset dinâmico** e **ganho ajustável** para compensar diferenças de hardware.  
   - Suaviza as leituras usando um **filtro exponencial α = 0.2**.

3. **Estados do Poço**
   - `POCO_VAZIO` → ambos sensores secos.  
   - `POCO_ENCHENDO` → inferior molhado, superior seco.  
   - `POCO_CHEIO` → ambos molhados.  
   - `POCO_DESCENDO` → inferior molhado, superior seco (após cheio).  
   - `SENSOR_FALHA` → inconsistente (inferior seco e superior molhado).

4. **Controle da Bomba**
   - Liga a bomba quando o poço está cheio ou descendo.  
   - Desliga quando vazio ou enchendo.  
   - Em caso de falha no sensor, a bomba é automaticamente **bloqueada**.

5. **Calibração**
   - Botão dedicado permite ajustar ganhos durante operação normal.  
   - A calibração mede as tensões de ambos sensores imersos e equaliza o ganho.

6. **Operação Não Bloqueante**
   - Todas as medições usam `millis()` para garantir resposta contínua.  
   - O loop principal roda sem travar o microcontrolador.

---

## 📊 Fluxograma Simplificado

Início
│
├──> Verifica botões
│
├──> Leitura sensores (A1/A2)
│
├──> Determina estado do poço
│
├──> Atualiza bomba (liga/desliga)
│
└──> Envia dados ao Serial Plotter


## ⚙️ Calibração Automática dos Sensores

1. Mantenha **os eletrodos imersos em água**.  
2. Pressione e segure o **botão de modo** por 2 s ao ligar.  
3. O sistema mede as tensões médias e ajusta automaticamente os ganhos:  
   
   float alvo = (vmin + vmax) / 2.0;
   ganhoMin = alvo / vmin;
   ganhoMax = alvo / vmax;

