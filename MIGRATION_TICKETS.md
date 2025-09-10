# Migration des Contrats - Élimination Duplication de Code

## Vue d'ensemble
Migration des contrats pour éliminer la duplication de code (~50%) en implémentant un système GameResult unifié. Cette migration transforme les paires `action()` / `try_action()` dupliquées en une implémentation unique avec des wrappers minimaux.

**Objectif**: Réduire le code de ~1800 lignes à ~1000 lignes tout en améliorant la maintenabilité.

## ⚠️ **NOTE IMPORTANTE - DEBUGGING**
Les fonctions `sozo execute` ne montrent PAS les valeurs de retour. **TOUJOURS utiliser `println!` pour debug** :
```cairo
fn ma_fonction(ref self: ContractState) -> u32 {
    let result = some_computation();
    println!("DEBUG: result = {}", result); // OBLIGATOIRE
    result
}
```
**Sans println!, vous ne verrez jamais si vos fonctions marchent !** 🚨

---

## 📋 Liste des Tickets

### Status Legend
- ⬜ **TODO**: Pas commencé
- 🟡 **IN PROGRESS**: En cours  
- ✅ **DONE**: Terminé
- ❌ **BLOCKED**: Bloqué

---

## Ticket #1: Création du système GameResult
**Status**: ⬜ **TODO**  
**Estimation**: 1 jour  
**Priorité**: CRITIQUE (bloquant pour tous les autres)

### Description
Créer le système d'erreurs unifié qui servira de base à toutes les migrations suivantes.

### Tâches à réaliser
- [ ] Créer le fichier `src/common/errors.cairo`
- [ ] Implémenter l'enum `GameResult<T>` 
- [ ] Implémenter l'enum `GameError` avec tous les cas d'erreur
- [ ] Créer le trait `GameResultTrait<T>` avec méthodes `is_ok()`, `is_err()`, `unwrap()`
- [ ] Créer la fonction `format_error()` pour convertir les erreurs en messages
- [ ] Ajouter le module `errors` dans `src/lib.cairo`

### Code à implémenter
```cairo
// src/common/errors.cairo
#[derive(Drop, Serde)]
enum GameResult<T> {
    Ok: T,
    Err: GameError,
}

#[derive(Drop, Serde)]
enum GameError {
    PlayerLocked: u64,                    // unlock_time
    InsufficientItems: (u16, u32),        // (item_id, needed_quantity)
    InvalidPosition: (u64, u64, u64),     // (x, y, z)
    ResourceNotFound,
    InsufficientSpace,
    InvalidInput: ByteArray,
    ChunkNotFound,
    StructureNotFound,
    ProcessingInProgress,
    InsufficientFunds: u32,               // needed_amount
}

trait GameResultTrait<T> {
    fn is_ok(self: @GameResult<T>) -> bool;
    fn is_err(self: @GameResult<T>) -> bool;
    fn unwrap(self: GameResult<T>) -> T;
}

fn format_error(error: GameError) -> ByteArray {
    match error {
        GameError::PlayerLocked(unlock_time) => "Player locked until " + unlock_time.to_string(),
        GameError::InsufficientItems((item_id, needed)) => "Need " + needed.to_string() + " of item " + item_id.to_string(),
        // ... autres cas
    }
}
```

### Tests de validation
- [ ] Compiler le contrat sans erreur
- [ ] Vérifier que `GameResult::Ok(42).is_ok()` retourne `true`
- [ ] Vérifier que `GameResult::Err(GameError::ResourceNotFound).is_err()` retourne `true`
- [ ] Tester `unwrap()` sur Ok et Err
- [ ] Vérifier que `format_error()` retourne des messages lisibles

**⚠️ IMPORTANT - Debug avec println!** :
Les fonctions Dojo ne retournent pas visiblement les valeurs dans sozo execute. **TOUJOURS ajouter des `println!` pour debug** :
```cairo
fn test_gameresult_success(ref self: ContractState) -> u32 {
    let result: GameResult<u32> = GameResult::Ok(42);
    let value = result.unwrap();
    println!("GameResult success test: {}", value); // OBLIGATOIRE pour voir le résultat
    value
}
```

---

## Ticket #2: Migration hit_block (Action pilote)
**Status**: ⬜ **TODO**  
**Estimation**: 1 jour  
**Priorité**: HAUTE  
**Dépend de**: Ticket #1

### Description
Migrer la première action `hit_block` / `try_hit_block` pour valider l'approche et servir de modèle.

### Tâches à réaliser
- [ ] Créer `hit_block_internal()` avec retour `GameResult<()>`
- [ ] Remplacer tous les `assert!` par des `return GameResult::Err`
- [ ] Créer les wrappers `hit_block()` et `try_hit_block()`
- [ ] Supprimer l'ancienne implémentation dupliquée
- [ ] Mettre à jour les tests existants

### Code à modifier
Dans `src/systems/actions.cairo`:

```cairo
impl ActionsImpl of ActionsContract {
    // Nouvelle implémentation interne
    fn hit_block_internal(ref self: ContractState, x: u64, y: u64, z: u64) -> GameResult<()> {
        let player = get_caller_address();
        let world = self.world();
        
        // Validation player lock - REMPLACER assert! par return Err
        let lock_data: PlayerLockData = world.read_model(player);
        if starknet::get_block_timestamp() < lock_data.unlock_time {
            return GameResult::Err(GameError::PlayerLocked(lock_data.unlock_time));
        }
        
        // ... toute la logique métier existante ...
        // Remplacer tous les assert! par return GameResult::Err(...)
        
        GameResult::Ok(())
    }
    
    // Wrappers minimaux
    fn hit_block(ref self: ContractState, x: u64, y: u64, z: u64) {
        self.hit_block_internal(x, y, z).unwrap();
    }
    
    fn try_hit_block(ref self: ContractState, x: u64, y: u64, z: u64) -> bool {
        self.hit_block_internal(x, y, z).is_ok()
    }
}
```

### Tests de validation
- [ ] Compiler sans erreur
- [ ] `hit_block()` avec paramètres valides → succès
- [ ] `hit_block()` avec joueur locked → panic avec message d'erreur correct
- [ ] `try_hit_block()` avec paramètres valides → `true`
- [ ] `try_hit_block()` avec joueur locked → `false`
- [ ] Tester en jeu: frapper un bloc fonctionne normalement
- [ ] Tester les batch operations qui utilisent `try_hit_block`

---

## Ticket #3: Migration des actions d'inventaire
**Status**: ⬜ **TODO**  
**Estimation**: 1.5 jours  
**Priorité**: HAUTE  
**Dépend de**: Ticket #2

### Description
Migrer toutes les actions liées à l'inventaire qui ont des variantes safe/unsafe.

### Actions à migrer
- [ ] `use_item()` / `try_use_item()`
- [ ] `drop_item()` / `try_drop_item()`  
- [ ] `take_item()` / `try_take_item()`
- [ ] `move_item()` / `try_move_item()`

### Tâches par action
1. Créer `{action}_internal()` avec `GameResult`
2. Remplacer `assert!` par `return GameResult::Err`
3. Créer wrappers unsafe et safe
4. Supprimer ancienne implémentation
5. Tester

### Tests de validation
- [ ] Toutes les actions compilent sans erreur
- [ ] Test fonctionnel pour chaque action (version unsafe et safe)
- [ ] Vérifier que l'inventaire fonctionne correctement en jeu
- [ ] Vérifier les optimistic updates côté client
- [ ] Test des cas d'erreur: inventaire plein, item inexistant, etc.

---

## Ticket #4: Migration des actions de monde/blocs
**Status**: ⬜ **TODO**  
**Estimation**: 1.5 jours  
**Priorité**: MOYENNE  
**Dépend de**: Ticket #3

### Description
Migrer les actions de placement/interaction avec le monde.

### Actions à migrer
- [ ] `place_block()` / `place_block_safe()`
- [ ] `harvest()` / `try_harvest()`
- [ ] `plant()` / `try_plant()`
- [ ] `open_container()` / `try_open_container()`

### Tests de validation
- [ ] Placement de blocs fonctionne
- [ ] Harvest des ressources fonctionne
- [ ] Plantation fonctionne
- [ ] Ouverture de conteneurs fonctionne
- [ ] Test des cas d'erreur: position invalide, pas de ressource, etc.

---

## Ticket #5: Migration des actions économiques
**Status**: ⬜ **TODO**  
**Estimation**: 1 jour  
**Priorité**: MOYENNE  
**Dépend de**: Ticket #4

### Description
Migrer les actions liées à l'économie et au commerce.

### Actions à migrer
- [ ] `buy()` / `try_buy()`
- [ ] `sell()` / `try_sell()`
- [ ] `give_item()` / `try_give_item()`

### Tests de validation
- [ ] Achat d'items fonctionne
- [ ] Vente d'items fonctionne  
- [ ] Don d'items entre joueurs fonctionne
- [ ] Test des cas d'erreur: fonds insuffisants, items inexistants

---

## Ticket #6: Migration crafting et processing
**Status**: ⬜ **TODO**  
**Estimation**: 1.5 jours  
**Priorité**: MOYENNE  
**Dépend de**: Ticket #5

### Description
Migrer les actions de craft et de processing qui sont plus complexes.

### Actions à migrer
- [ ] `craft()` / `try_craft()`
- [ ] `craft_bulk()` / `try_craft_bulk()` 
- [ ] `start_processing()` / `try_start_processing()`
- [ ] `finish_processing()` / `try_finish_processing()`

### Tests de validation
- [ ] Craft simple fonctionne
- [ ] Craft en bulk fonctionne
- [ ] Processing start/finish fonctionne
- [ ] Test des cas d'erreur: ressources manquantes, processing en cours

---

## Ticket #7: Migration actions diverses et cleanup
**Status**: ⬜ **TODO**  
**Estimation**: 1 jour  
**Priorité**: BASSE  
**Dépend de**: Ticket #6

### Description
Migrer les dernières actions et faire le cleanup final.

### Actions restantes
- [ ] `teleport()` / `try_teleport()`
- [ ] `set_name()` (pas de variante try, mais utiliser GameResult en interne)
- [ ] Autres actions spéciales

### Cleanup
- [ ] Supprimer tout le code mort
- [ ] Vérifier qu'aucune fonction n'appelle les anciennes versions
- [ ] Optimiser les imports
- [ ] Vérifier la cohérence des messages d'erreur

### Tests de validation
- [ ] Compile sans warnings
- [ ] Toutes les fonctionnalités du jeu marchent
- [ ] Aucune régression détectée
- [ ] Performance équivalente ou meilleure

---

## Ticket #8: Tests d'intégration et documentation
**Status**: ⬜ **TODO**  
**Estimation**: 0.5 jour  
**Priorité**: BASSE  
**Dépend de**: Ticket #7

### Description
Tests finaux et mise à jour de la documentation.

### Tâches
- [ ] Test d'intégration complet du jeu
- [ ] Vérifier que les batch operations fonctionnent
- [ ] Test de charge sur les nouvelles fonctions
- [ ] Mettre à jour la documentation des erreurs
- [ ] Créer guide de migration pour futures actions

### Tests de validation
- [ ] Session de jeu complète sans bug
- [ ] Batch operations performantes  
- [ ] Messages d'erreur clairs pour les utilisateurs
- [ ] Documentation à jour

---

## 📊 Métriques de Validation

### Avant migration
- **Lignes de code**: ~1800 lignes dans actions.cairo
- **Fonctions dupliquées**: ~20 paires action/try_action
- **Maintienabilité**: Difficile (modifications en double)

### Après migration (cibles)
- **Lignes de code**: ~1000 lignes dans actions.cairo (-45%)
- **Fonctions dupliquées**: 0 (wrappers de 2-3 lignes seulement)
- **Maintienabilité**: Excellente (une seule implémentation par action)

### Tests de performance
- [ ] Temps de compilation identique ou meilleur
- [ ] Gas cost équivalent ou meilleur
- [ ] Pas de régression de performance

---

## 🚨 Risques et Mitigation

### Risques identifiés
1. **Régression fonctionnelle**: Une action ne marche plus après migration
   - **Mitigation**: Tests systématiques après chaque ticket
   
2. **Performance dégradée**: Le GameResult ajoute de l'overhead
   - **Mitigation**: Benchmarks avant/après
   
3. **Client cassé**: Les appels depuis le client UE5 ne marchent plus
   - **Mitigation**: Garder les signatures identiques

### Plan de rollback
- Garder une branche `backup-before-migration` 
- Possibilité de rollback par ticket individuel
- Tests d'intégration obligatoires avant merge

---

## 📞 Contacts et Support

**Lead technique**: [Nom du lead]  
**Stagiaire assigné**: [Nom du stagiaire]  
**Timeline estimée**: 8-10 jours de travail  
**Deadline**: [Date cible]

**Questions/Blocages**: Créer une issue GitHub ou contacter le lead technique.