# Processamento Multicore com RP2040 na BitDogLab

__Atividade Multicore__<br>
Repositório foi criado com o intuito de desenvolver um projeto empregando o uso dos dois núcleos disponíveis no RP2040.<br>

__Responsável pelo desenvolvimento:__
Andressa Sousa Fonseca

## Detalhamento Do Projeto

### Funcionalidade do Sistema

No projeto desenvolvido, são utilizados dois sensores, o BMP280 e o AHT20, responsáveis por capturar a pressão e a umidade do ambiente, respectivamente. A leitura contínua 
dos sensores é realizada pelo core 0 do RP2040. Visando o paralelismo do processamento, o núcleo core 1 foi utilizado para realizar a exibição dos dados de leitura. Nesse aspecto, foi
de suma importância a utilização da FIFO para comunicação entre os dois núcleos.
